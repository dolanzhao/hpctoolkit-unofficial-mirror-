// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2017, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

// This file builds and prints an hpcstruct file with loops and inline
// sequences based on input from the ParseAPI Control-Flow Graph
// support in Dyninst.  We no longer use Open Analysis or the
// Prof::Struct classes.
//
// The internal representation of the structure info is defined in two
// files:
//
//   Struct-Skel.hpp -- defines the top-level skeleton classes for
//   Files, Groups and Procedures (Functions).
//
//   Struct-Inline.hpp -- defines the Inline Tree classes for Loops,
//   Statements (Instructions) and inline sequences.
//
// Printing the internal objects in the hpcstruct XML format is
// handled in Struct-Output.cpp.

//***************************************************************************

#include <sys/types.h>
#include <include/uint.h>

#include <map>
#include <set>
#include <string>
#include <vector>
#include <ostream>
#include <sstream>

#include <lib/binutils/BinUtils.hpp>
#include <lib/binutils/VMAInterval.hpp>
#include <lib/support/FileNameMap.hpp>
#include <lib/support/FileUtil.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/support/StringTable.hpp>

#include <CFG.h>
#include <CodeObject.h>
#include <CodeSource.h>
#include <Function.h>
#include <Instruction.h>
#include <Module.h>
#include <Symtab.h>

#include "Linemap.hpp"
#include "Struct.hpp"
#include "Struct-Inline.hpp"
#include "Struct-Output.hpp"
#include "Struct-Skel.hpp"

using namespace Dyninst;
using namespace Inline;
using namespace InstructionAPI;
using namespace SymtabAPI;
using namespace ParseAPI;
using namespace std;

#define USE_DYNINST_LINE_MAP    1
#define USE_LIBDWARF_LINE_MAP   0

#define ENABLE_PARSEAPI_GAPS  1

#define DEBUG_CFG_SOURCE  0
#define DEBUG_MAKE_SKEL   0

// Copied from lib/prof/Struct-Tree.cpp
static const string & unknown_file = "<unknown file>";
static const string & unknown_proc = "<unknown proc>";
static const string & unknown_link = "_unknown_proc_";

// FIXME: temporary until the line map problems are resolved
static Symtab * the_symtab = NULL;

#if USE_LIBDWARF_LINE_MAP
static LineMap * the_linemap = NULL;
#endif

//----------------------------------------------------------------------

namespace BAnal {
namespace Struct {

class HeaderInfo;

typedef map <Block *, bool> BlockSet;
typedef map <VMA, HeaderInfo> HeaderList;
typedef vector <Statement::Ptr> StatementVector;

static FileMap *
makeSkeleton(CodeObject *, ProcNameMgr *);

static void
doFunctionList(Symtab *, FileInfo *, GroupInfo *, HPC::StringTable &);

static LoopList *
doLoopTree(FileInfo *, GroupInfo *, ParseAPI::Function *,
	   BlockSet &, LoopTreeNode *, HPC::StringTable &);

static TreeNode *
doLoopLate(GroupInfo *, ParseAPI::Function *, BlockSet &, Loop *,
	   const string &, HPC::StringTable &);

static void
doBlock(GroupInfo *, ParseAPI::Function *, BlockSet &, Block *,
	TreeNode *, HPC::StringTable &);

static void
doCudaFunction(GroupInfo * ginfo, ParseAPI::Function * func, TreeNode * root,
	       HPC::StringTable & strTab);

static void
getStatement(StatementVector &, Offset, SymtabAPI::Function *);

static LoopInfo *
findLoopHeader(FileInfo *, GroupInfo *, ParseAPI::Function *,
	       TreeNode *, Loop *, const string &, HPC::StringTable &);

static TreeNode *
deleteInlinePrefix(TreeNode *, Inline::InlineSeqn, HPC::StringTable &);

static void
computeGaps(VMAIntervalSet &, VMAIntervalSet &, VMA, VMA);

#if DEBUG_CFG_SOURCE

#define DEBUG_MESG(expr)  std::cout << expr

static string
debugPrettyName(const string &);

static void
debugStmt(VMA, int, string &, SrcFile::ln);

static void
debugLoop(GroupInfo *, ParseAPI::Function *, Loop *, const string &,
	  vector <Edge *> &, HeaderList &);

static void
debugInlineTree(TreeNode *, LoopInfo *, HPC::StringTable &, int, bool);

#else

#define DEBUG_MESG(expr)

#endif  // DEBUG_CFG_SOURCE

// Info on candidates for loop header.
class HeaderInfo {
public:
  Block * block;
  bool  is_src;
  bool  is_targ;
  bool  is_cond;
  bool  in_incl;
  bool  in_excl;
  int   depth;
  int   score;

  HeaderInfo(Block * blk = NULL)
  {
    block = blk;
    is_src = false;
    is_targ = false;
    is_cond = false;
    in_incl = false;
    in_excl = false;
    depth = 0;
    score = 0;
  }
};

//----------------------------------------------------------------------

// Make a dot (graphviz) file for the Control-Flow Graph for each
// procedure and write to the ostream 'dotFile'.
//
static void
makeDotFile(std::ostream * dotFile, CodeObject * code_obj)
{
  const CodeObject::funclist & funcList = code_obj->funcs();

  for (auto fit = funcList.begin(); fit != funcList.end(); ++fit)
  {
    ParseAPI::Function * func = *fit;
    map <Block *, int> blockNum;
    map <Block *, int>::iterator mit;
    int num;

    *dotFile << "--------------------------------------------------\n"
	     << "Procedure: '" << func->name() << "'\n\n"
	     << "digraph " << func->name() << " {\n"
	     << "  1 [ label=\"start\" shape=\"diamond\" ];\n";

    const ParseAPI::Function::blocklist & blist = func->blocks();

    // write the list of nodes (blocks)
    num = 1;
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      num++;

      blockNum[block] = num;
      *dotFile << "  " << num << " [ label=\"0x" << hex << block->start()
	       << dec << "\" ];\n";
    }
    int endNum = num + 1;
    *dotFile << "  " << endNum << " [ label=\"end\" shape=\"diamond\" ];\n";

    // in parseAPI, functions have a unique entry point
    mit = blockNum.find(func->entry());
    if (mit != blockNum.end()) {
      *dotFile << "  1 -> " << mit->second << ";\n";
    }

    // write the list of internal edges
    num = 1;
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      const ParseAPI::Block::edgelist & elist = block->targets();
      num++;

      for (auto eit = elist.begin(); eit != elist.end(); ++eit) {
	mit = blockNum.find((*eit)->trg());
	if (mit != blockNum.end()) {
	  *dotFile << "  " << num << " -> " << mit->second << ";\n";
	}
      }
    }

    // add any exit edges
    const ParseAPI::Function::const_blocklist & eblist = func->exitBlocks();
    for (auto bit = eblist.begin(); bit != eblist.end(); ++bit) {
      Block * block = *bit;
      mit = blockNum.find(block);
      if (mit != blockNum.end()) {
	*dotFile << "  " << mit->second << " -> " << endNum << ";\n";
      }
    }
  }

    *dotFile << "}\n" << endl;
}


//----------------------------------------------------------------------

// makeStructure -- the main entry point for hpcstruct realmain().
//
// Read the binutils load module and the parseapi code object, iterate
// over functions, loops and blocks, make an internal inline tree and
// write an hpcstruct file to 'outFile'.  Also, write a dot (graphviz)
// file if 'dotFile' is non-null.
//
void
makeStructure(string filename,
	      string gaps_filenm,
	      ostream * outFile,
	      ostream * gapsFile,
	      ostream * dotFile,
	      ProcNameMgr * procNmMgr)
{
  HPC::StringTable strTab;

  InputFile inputFile;

  ElfFileVector *elfFileVector = inputFile.openFile(filename);

  if (elfFileVector && !elfFileVector->empty()) {
    Output::printStructFileBegin(outFile, gapsFile, filename);
    for (unsigned int i = 0; i < elfFileVector->size(); i++) {
      ElfFile *elfFile = (*elfFileVector)[i]; 

      // insert empty string "" first
      strTab.str2index("");

#if USE_LIBDWARF_LINE_MAP
      the_linemap = new LineMap;
      the_linemap->readFile(elfFile.getElf());
#endif

      Symtab * symtab = Inline::openSymtab(elfFile);
      the_symtab = symtab;

#if USE_DYNINST_LINE_MAP
      vector <Module *> modVec;
      the_symtab->getAllModules(modVec);

      for (auto mit = modVec.begin(); mit != modVec.end(); ++mit) {
	(*mit)->parseLineInformation();
      }
#endif

      SymtabCodeSource * code_src;
      CodeObject * code_obj = NULL;

      if (symtab != NULL) {
	code_src = new SymtabCodeSource(symtab);
	code_obj = new CodeObject(code_src);
	if (symtab->getArchitecture() != Dyninst::Arch_cuda) {
	  code_obj->parse();
	}
      }

      FileMap * fileMap = makeSkeleton(code_obj, procNmMgr);

      Output::printLoadModuleBegin(outFile, elfFile->getFileName());

      // process the files in the skeleton map
      for (auto fit = fileMap->begin(); fit != fileMap->end(); ++fit) {
	FileInfo * finfo = fit->second;

	Output::printFileBegin(outFile, finfo);

	// process the groups within one file
	for (auto git = finfo->groupMap.begin(); git != finfo->groupMap.end(); ++git) {
	  GroupInfo * ginfo = git->second;

	  // make the inline tree for all funcs in one group
	  doFunctionList(symtab, finfo, ginfo, strTab);

	  for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) {
	    ProcInfo * pinfo = pit->second;
	    Output::printProc(outFile, gapsFile, gaps_filenm,
			      finfo, ginfo, pinfo, strTab);

	    delete pinfo->root;
	    pinfo->root = NULL;
	  }
	}
	Output::printFileEnd(outFile, finfo);
      }

      Output::printLoadModuleEnd(outFile);

      // write CFG in dot (graphviz) format to file
      if (dotFile != NULL) {
	makeDotFile(dotFile, code_obj);
      }

      Inline::closeSymtab();
    }
    Output::printStructFileEnd(outFile, gapsFile);
  }
}

//----------------------------------------------------------------------

// makeSkeleton -- the new buildLMSkeleton
//
// In the ParseAPI version, we iterate over the ParseAPI list of
// functions and match them up with the SymtabAPI functions by entry
// address.  We now use Symtab for the line map info.
//
// Note: several parseapi functions (targ410aa7) may map to the same
// symtab proc, so we make a func list (group).
//
static FileMap *
makeSkeleton(CodeObject * code_obj, ProcNameMgr * procNmMgr)
{
  FileMap * fileMap = new FileMap;

  // iterate over the ParseAPI Functions
  const CodeObject::funclist & funcList = code_obj->funcs();

  for (auto flit = funcList.begin(); flit != funcList.end(); ++flit) {
    ParseAPI::Function * func = *flit;
    SymtabAPI::Function * sym_func = NULL;
    VMA  vma = func->addr();

    string  linknm = unknown_link;
    string  procnm = unknown_proc;
    string  filenm;
    SrcFile::ln  line = 0;

    bool found;
    if (the_symtab->getArchitecture() == Dyninst::Arch_cuda) {
      std::vector<SymtabAPI::Function *> fns;
      const std::string &mname = func->mangledName();
      found = the_symtab->findFunctionsByName(fns, mname, mangledName);
      if (found) {
	if (fns.size() > 1) {
	  std::cerr << "warning: more than one symtab function for '"
		    << func->name()
		    << "': ";
	  for (unsigned int i = 0; i < fns.size(); i++) {
	    SymtabAPI::Function *f = fns[i];
	    std::cerr << f->getName()
		      << " ";
	  }
	  std::cerr << std::endl;
	}
	sym_func = fns[0];
      }
    } else {
      found = the_symtab->getContainingFunction(vma, sym_func);
    }

    // find the symtab symbol where this func is
    if (found && sym_func != NULL) {
      auto mangled_it = sym_func->mangled_names_begin();
      auto typed_it = sym_func->typed_names_begin();
      VMA sym_vma = sym_func->getOffset();

      if (mangled_it != sym_func->mangled_names_end()) {
	linknm = *mangled_it;
      }
      if (typed_it != sym_func->typed_names_end()) {
	procnm = *typed_it;
      }

      vector <Statement::Ptr> svec;
      getStatement(svec, sym_vma, sym_func);

      if (! svec.empty()) {
	filenm = svec[0]->getFile();
	line = svec[0]->getLine();
      }
    }

    if (filenm != "") {
      RealPathMgr::singleton().realpath(filenm);
    }
    else {
      filenm = unknown_file;
    }

    //
    // locate in file map, or else insert
    //
    FileInfo * finfo = NULL;

    auto fit = fileMap->find(filenm);
    if (fit != fileMap->end()) {
      finfo = fit->second;
    }
    else {
      finfo = new FileInfo(filenm);
      (*fileMap)[filenm] = finfo;
    }

    //
    // insert into group map for this file
    //
    GroupInfo * ginfo = NULL;

    auto git = finfo->groupMap.find(sym_func);
    if (git != finfo->groupMap.end()) {
      ginfo = git->second;
    }
    else {
      ginfo = new GroupInfo(sym_func, linknm, procnm);
      finfo->groupMap[sym_func] = ginfo;
    }
    ginfo->procMap[vma] = new ProcInfo(func, NULL, line);
  }

#if DEBUG_MAKE_SKEL
  // print the skeleton map

  for (auto fit = fileMap->begin(); fit != fileMap->end(); ++fit) {
    auto finfo = fit->second;

    for (auto git = finfo->groupMap.begin(); git != finfo->groupMap.end(); ++git) {
      auto ginfo = git->second;
      long size = ginfo->procMap.size();
      long num = 0;

      for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) {
	auto pinfo = pit->second;
	num++;

	cout << "\nentry:   0x" << hex << pinfo->entry_vma << dec
	     << "  (" << num << "/" << size << ")\n"
	     << "symbol:  0x" << hex << ginfo->start
	     << "--0x" << ginfo->end << dec << "\n"
	     << "file:    " << finfo->fileName << "\n"
	     << "link:    " << ginfo->linkName << "\n"
	     << "pretty:  " << ginfo->prettyName << "\n"
	     << "parse:   " << pinfo->func->name() << "\n"
	     << "line:    " << pinfo->line_num << "\n";
      }
    }
  }
#endif

  return fileMap;
}

//****************************************************************************
// ParseAPI code for functions, loops and blocks
//****************************************************************************

// Remaining TO-DO items:
//
// 4. Irreducible loops -- study entry blocks, loop header, adjacent
// nested loops.  Some nested irreducible loops share entry blocks and
// maybe should be merged.
//
// 5. Compute line ranges for loops and procs to help decide what is
// alien code when the symtab inline info is not available.
//
// 6. Handle code movement wrt loops: loop fusion, fission, moving
// code in/out of loops.
//
// 7. Maybe write our own functions for coalescing statements and
// aliens and normalizing source code transforms.
//
// 10. Decide how to handle basic blocks that belong to multiple
// functions.
//
// 14. Import fix for duplicate proc names (pretty vs. typed/mangled).
//
// 15. Some missing file names are "~unknown-file~", some are "", they
// string match to not equal, causing a spurious alien.
//

//----------------------------------------------------------------------

// Process the functions in one binutils group.  For each ParseAPI
// function in the group, create the inline tree and fill in the
// TreeNode ptr in ProcInfo.
//
// One binutils proc may contain multiple embedded parseapi functions.
// In that case, we create new proc/file scope nodes for each function
// and strip the inline prefix at the call source from the embed
// function.  This often happens with openmp parallel pragmas.
//
static void
doFunctionList(Symtab * symtab, FileInfo * finfo, GroupInfo * ginfo,
	       HPC::StringTable & strTab)
{
  VMAIntervalSet covered;

  long num_funcs = ginfo->procMap.size();

  // make a map of internal call edges (from target to source) across
  // all funcs in this group.  we use this to strip the inline seqn at
  // the call source from the target func.
  //
  std::map <VMA, VMA> callMap;

  if (symtab->getArchitecture() != Dyninst::Arch_cuda) {
  if (num_funcs > 1) {
    for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) {
      ParseAPI::Function * func = pit->second->func;
      const ParseAPI::Function::edgelist & elist = func->callEdges();

      for (auto eit = elist.begin(); eit != elist.end(); ++eit) {
	VMA src = (*eit)->src()->last();
	VMA targ = (*eit)->trg()->start();
	callMap[targ] = src;
      }
    }
  }
  }

  // one binutils proc may contain several parseapi funcs
  long num = 0;
  for (auto pit = ginfo->procMap.begin(); pit != ginfo->procMap.end(); ++pit) {
    ProcInfo * pinfo = pit->second;
    ParseAPI::Function * func = pinfo->func;
    Address entry_addr = func->addr();
    num++;

    if (num == 1) { pinfo->leader = true; }

    // compute the inline seqn for the call site for this func, if
    // there is one.
    Inline::InlineSeqn prefix;
    auto call_it = callMap.find(entry_addr);

    if (call_it != callMap.end()) {
      Inline::analyzeAddr(prefix, call_it->second);
    }

#if DEBUG_CFG_SOURCE
    cout << "\n------------------------------------------------------------\n"
	 << "func:  0x" << hex << entry_addr << dec
	 << "  (" << num << "/" << num_funcs << ")"
	 << "  link='" << ginfo->linkName << "'\n"
	 << "parse:  '" << func->name() << "'\n"
	 << "file:   '" << finfo->fileName << "'\n";

    if (call_it != callMap.end()) {
      cout << "\ncall site prefix:  0x" << hex << call_it->second
	   << " -> 0x" << call_it->first << dec << "\n";
      for (auto pit = prefix.begin(); pit != prefix.end(); ++pit) {
	cout << "inline:  l=" << pit->getLineNum()
	     << "  f='" << pit->getFileName()
	     << "'  p='" << debugPrettyName(pit->getProcName()) << "'\n";
      }
    }
#endif

    if (symtab->getArchitecture() != Dyninst::Arch_cuda) {
    // if this function is entirely contained within another function
    // (as determined by its entry block), then skip it.
    if (num_funcs > 1 && func->entry()->containingFuncs() > 1) {
      DEBUG_MESG("\nskipping duplicated function:  '" << func->name() << "'\n");
      continue;
    }
    }

    TreeNode * root = new TreeNode;

    // initially, create an empty blocklist for cuda
    ParseAPI::Function::blocklist emptyBlockList; 
    ParseAPI::Function::blocklist &blist = emptyBlockList;

    if (symtab->getArchitecture() != Dyninst::Arch_cuda) {
    // basic blocks for this function
    blist = func->blocks();
    BlockSet visited;

    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      visited[block] = false;
    }

#if ENABLE_PARSEAPI_GAPS
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      covered.insert(block->start(), block->end());
    }
#endif

    // traverse the loop (Tarjan) tree
    LoopList *llist =
	doLoopTree(finfo, ginfo, func, visited, func->getLoopTree(), strTab);

#if DEBUG_CFG_SOURCE
    cout << "\nnon-loop blocks:\n";
#endif

    // process any blocks not in a loop
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
      Block * block = *bit;
      if (! visited[block]) {
	doBlock(ginfo, func, visited, block, root, strTab);
      }
    }

    // merge the loops into the proc's inline tree
    FLPSeqn empty;

    for (auto it = llist->begin(); it != llist->end(); ++it) {
      mergeInlineLoop(root, empty, *it);
    }
    } else {
      doCudaFunction(ginfo, func, root, strTab);
    }

    // delete the inline prefix from this func, if non-empty
    if (! prefix.empty()) {
      root = deleteInlinePrefix(root, prefix, strTab);
    }

    // add inline tree to proc info
    pinfo->root = root;

#if DEBUG_CFG_SOURCE
    cout << "\nfinal inline tree:  (" << num << "/" << num_funcs << ")"
	 << "  link='" << ginfo->linkName << "'\n"
	 << "parse:  '" << func->name() << "'\n";

    if (call_it != callMap.end()) {
      cout << "\ncall site prefix:  0x" << hex << call_it->second
	   << " -> 0x" << call_it->first << dec << "\n";
      for (auto pit = prefix.begin(); pit != prefix.end(); ++pit) {
	cout << "inline:  l=" << pit->getLineNum()
	     << "  f='" << pit->getFileName()
	     << "'  p='" << debugPrettyName(pit->getProcName()) << "'\n";
      }
    }
    cout << "\n";
    debugInlineTree(root, NULL, strTab, 0, true);
    cout << "\nend proc:  (" << num << "/" << num_funcs << ")"
	 << "  link='" << ginfo->linkName << "'\n"
	 << "parse:  '" << func->name() << "'\n";
#endif
  }

#if ENABLE_PARSEAPI_GAPS
  if (symtab->getArchitecture() != Dyninst::Arch_cuda) {
  VMAIntervalSet gaps;
  computeGaps(covered, gaps, ginfo->start, ginfo->end);

  for (auto git = gaps.begin(); git != gaps.end(); ++git) {
    ginfo->gapList.push_back(GapInfo(git->beg(), git->end(), 0));
  }

#if 0
  auto pit = ginfo->procMap.begin();
  ProcInfo * pinfo = pit->second;

  cout << "\n------------------------------------------------------------\n"
       << "func:  0x" << hex << pinfo->entry_vma << dec
       << "  (" << num_funcs << ")\n"
       << "link:   " << ginfo->linkName << "\n"
       << "parse:  " << pinfo->func->name() << "\n"
       << "0x" << hex << ginfo->start
       << "--0x" << ginfo->end << dec << "\n";

  cout << "\ncovered:\n"
       << covered.toString() << "\n"
       << "\ngaps:\n"
       << gaps.toString() << "\n";
#endif
  }
#endif
}

//----------------------------------------------------------------------

#if 0
// Keep me (for now): may want to integrate parts of this code into
// the all-parseapi view of the world.
//
// Locate the beginning line number and VMA range for the inline tree
// 'root' and make new Proc and File scope nodes for this function.
// This is only for multiple parseAPI funcs within one binutils proc.
//
// Returns: proc scope node
//
static Prof::Struct::Proc *
makeProcScope(Prof::Struct::LM * lm, ProcInfo * pinfo, ParseAPI::Function * func,
	      TreeNode * root, const ParseAPI::Function::blocklist & blist,
	      HPC::StringTable & strTab)
{
  long empty_index = strTab.str2index("");
  long file_index = empty_index;
  SrcFile::ln beg_line = 0;

  // locate the func at the file/line of the lowest VMA among root's
  // stmts (no inline steps) that has a non-empty file.  fall back on
  // the original binutils proc name (eg, no debug info).
  //
  for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
    StmtInfo *sinfo = sit->second;

    if (sinfo->file_index != empty_index && sinfo->line_num != 0) {
      file_index = sinfo->file_index;
      beg_line = sinfo->line_num;
      break;
    }
  }

  string filenm = (file_index != empty_index) ?
      strTab.index2str(file_index) : pinfo->proc_bin->filename();
  string basenm = FileUtil::basename(filenm.c_str());
  stringstream buf;

  buf << "outline " << basenm << ":" << beg_line
      << " (0x" << hex << func->addr() << dec << ")";

  Prof::Struct::File * fileScope = Prof::Struct::File::demand(lm, filenm);
  Prof::Struct::Proc * procScope =
      Prof::Struct::Proc::demand(fileScope, buf.str(), func->name(),
				 beg_line, beg_line, NULL);

  // scan the func's basic blocks and set min/max VMA
  VMA beg_vma = 0;
  VMA end_vma = 0;

  auto bit = blist.begin();
  if (bit != blist.end()) {
    beg_vma = (*bit)->start();
    end_vma = (*bit)->end();
  }
  for (; bit != blist.end(); ++bit) {
    beg_vma = std::min(beg_vma, (VMA) (*bit)->start());
    end_vma = std::max(end_vma, (VMA) (*bit)->end());
  }
  procScope->vmaSet().insert(beg_vma, end_vma);

  return procScope;
}
#endif

//----------------------------------------------------------------------

// Returns: list of LoopInfo objects.
//
// If the loop at this node is non-null (internal node), then the list
// contains one element for that loop.  If the loop is null (root),
// then the list contains one element for each subtree.
//
static LoopList *
doLoopTree(FileInfo * finfo, GroupInfo * ginfo, ParseAPI::Function * func,
	   BlockSet & visited, LoopTreeNode * ltnode,
	   HPC::StringTable & strTab)
{
  LoopList * myList = new LoopList;

  if (ltnode == NULL) {
    return myList;
  }
  Loop *loop = ltnode->loop;

  // process the children of the loop tree
  vector <LoopTreeNode *> clist = ltnode->children;

  for (uint i = 0; i < clist.size(); i++) {
    LoopList *subList =
	doLoopTree(finfo, ginfo, func, visited, clist[i], strTab);

    for (auto sit = subList->begin(); sit != subList->end(); ++sit) {
      myList->push_back(*sit);
    }
    delete subList;
  }

  // if no loop at this node (root node), then return the list of
  // children.
  if (loop == NULL) {
    return myList;
  }

  // otherwise, finish this loop, put into LoopInfo format, insert the
  // subloops and return a list of one.
  string loopName = ltnode->name();
  FLPSeqn empty;

  TreeNode * myLoop =
      doLoopLate(ginfo, func, visited, loop, loopName, strTab);

  for (auto it = myList->begin(); it != myList->end(); ++it) {
    mergeInlineLoop(myLoop, empty, *it);
  }

  // reparent the tree and put into LoopInfo format
  LoopInfo * myInfo =
      findLoopHeader(finfo, ginfo, func, myLoop, loop, loopName, strTab);

  myList->clear();
  myList->push_back(myInfo);

  return myList;
}

//----------------------------------------------------------------------

// Post-order process for one loop, after the subloops.  Add any
// leftover inclusive blocks, select the loop header, reparent as
// needed, remove the top inline spine, and put into the LoopInfo
// format.
//
// Returns: the raw inline tree for this loop.
//
static TreeNode *
doLoopLate(GroupInfo * ginfo, ParseAPI::Function * func,
	   BlockSet & visited, Loop * loop, const string & loopName,
	   HPC::StringTable & strTab)
{
  TreeNode * root = new TreeNode;

  DEBUG_MESG("\nbegin loop:  " << loopName << "  '"
	     << func->name() << "'\n");

  // add the inclusive blocks not contained in a subloop
  vector <Block *> blist;
  loop->getLoopBasicBlocks(blist);

  for (uint i = 0; i < blist.size(); i++) {
    if (! visited[blist[i]]) {
      doBlock(ginfo, func, visited, blist[i], root, strTab);
    }
  }

  return root;
}

//----------------------------------------------------------------------

// Process one basic block.
//
static void
doBlock(GroupInfo * ginfo, ParseAPI::Function * func,
	BlockSet & visited, Block * block, TreeNode * root,
	HPC::StringTable & strTab)
{
  if (block == NULL || visited[block]) {
    return;
  }
  visited[block] = true;

#if DEBUG_CFG_SOURCE
  cout << "\nblock:\n";
#endif

#if USE_DYNINST_LINE_MAP
  // save the last symtab line map query
  Offset low_vma = 1;
  Offset high_vma = 0;
  string cache_filenm = "";
  SrcFile::ln cache_line = 0;
#endif

  // iterate through the instructions in this block
  map <Offset, Instruction::Ptr> imap;
  block->getInsns(imap);

  for (auto iit = imap.begin(); iit != imap.end(); ++iit) {
    Offset vma = iit->first;
    int    len = iit->second->size();
    string filenm = "";
    SrcFile::ln line = 0;

#if USE_LIBDWARF_LINE_MAP
    LineRange lr;

    the_linemap->getLineRange(vma, lr);
    filenm = lr.filenm;
    line = lr.lineno;
#endif

#if USE_DYNINST_LINE_MAP
    if (low_vma <= vma && vma < high_vma) {
      // use cached value
      filenm = cache_filenm;
      line = cache_line;
    }
    else {
      StatementVector svec;
      getStatement(svec, vma, ginfo->sym_func);

      if (! svec.empty()) {
	// use symtab value and save in cache
	low_vma = svec[0]->startAddr();
	high_vma = svec[0]->endAddr();
	filenm = svec[0]->getFile();
	RealPathMgr::singleton().realpath(filenm);
	line = svec[0]->getLine();
	cache_filenm = filenm;
	cache_line = line;
      }
    }
#endif

#if DEBUG_CFG_SOURCE
    debugStmt(vma, len, filenm, line);
#endif

    addStmtToTree(root, strTab, vma, len, filenm, line);
  }
}

// process a cuda function
static void
doCudaFunction(GroupInfo * ginfo, ParseAPI::Function * func, TreeNode * root,
	       HPC::StringTable & strTab)
{

#if DEBUG_CFG_SOURCE
  cout << "\nblock:\n";
#endif

#if USE_DYNINST_LINE_MAP
  // save the last symtab line map query
  Offset low_vma = 1;
  Offset high_vma = 0;
  string cache_filenm = "";
  SrcFile::ln cache_line = 0;
  int try_symtab = 1;
#endif

  for (Offset vma = ginfo->start; vma < ginfo->end; vma += 4) {
    int    len = 4;
    string filenm = "";
    SrcFile::ln line = 0;

#if USE_LIBDWARF_LINE_MAP
    LineRange lr;

    the_linemap->getLineRange(vma, lr);
    filenm = lr.filenm;
    line = lr.lineno;
#endif

#if USE_DYNINST_LINE_MAP
    if (low_vma <= vma && vma < high_vma) {
      // use cached value
      filenm = cache_filenm;
      line = cache_line;
    }
    else {
      StatementVector svec;
      getStatement(svec, vma, ginfo->sym_func);

#if USE_FULL_SYMTAB_BACKUP
      // fall back on full symtab if module LineInfo fails.
      // symtab lookups are very expensive, so limit to one failure
      // per block.
      if (svec.empty() && try_symtab) {
	the_symtab->getSourceLines(svec, vma);
	if (svec.empty()) {
	  try_symtab = 0;
	}
      }
#endif
      if (! svec.empty()) {
	// use symtab value and save in cache
	low_vma = svec[0]->startAddr();
	high_vma = svec[0]->endAddr();
	filenm = getRealPath(svec[0]->getFile().c_str());
	line = svec[0]->getLine();
	cache_filenm = filenm;
	cache_line = line;
      }
    }
#endif

#if DEBUG_CFG_SOURCE
    debugStmt(vma, len, filenm, line);
#endif

    addStmtToTree(root, strTab, vma, len, filenm, line);
  }
}


//****************************************************************************
// Support functions
//****************************************************************************

// Line map info from SymtabAPI.  Try the Module associated with the
// Symtab Function as a hint first, else look for other modules that
// might contain vma.
//
static void
getStatement(StatementVector & svec, Offset vma, SymtabAPI::Function * sym_func)
{
  svec.clear();

  // try the Module in sym_func first as a hint
  if (sym_func != NULL) {
    Module * mod = sym_func->getModule();

    if (mod != NULL) {
      mod->getSourceLines(svec, vma);
    }
  }

  // else look for other modules
  if (svec.empty()) {
    set <Module *> modSet;
    the_symtab->findModuleByOffset(modSet, vma);

    for (auto mit = modSet.begin(); mit != modSet.end(); ++mit) {
      (*mit)->getSourceLines(svec, vma);
      if (! svec.empty()) {
	break;
      }
    }
  }

  // make sure file and line are either both known or both unknown.
  // this case probably never happens, but we do want to rely on it.
  if (! svec.empty()
      && (svec[0]->getFile() == "" || svec[0]->getLine() == 0))
  {
    svec.clear();
  }
}

//----------------------------------------------------------------------


// New heuristic for identifying loop header inside inline tree.
// Start at the root, descend the inline tree and try to find where
// the loop begins.  This is the central problem of all hpcstruct:
// where to locate a loop inside an inline sequence.
//
// Note: loop "headers" are no longer tied to a specific VMA and
// machine instruction.  They are strictly file and line number.
// (For some loops, there is no right VMA.)
//
// Returns: detached LoopInfo object.
//
static LoopInfo *
findLoopHeader(FileInfo * finfo, GroupInfo * ginfo, ParseAPI::Function * func,
	       TreeNode * root, Loop * loop, const string & loopName,
	       HPC::StringTable & strTab)
{
  long base_index = strTab.str2index(FileUtil::basename(finfo->fileName));
  string procName = func->name();

  //------------------------------------------------------------
  // Step 1 -- build the list of loop exit conditions
  //------------------------------------------------------------

  vector <Block *> inclBlocks;
  set <Block *> bset;
  HeaderList clist;

  loop->getLoopBasicBlocks(inclBlocks);
  for (auto bit = inclBlocks.begin(); bit != inclBlocks.end(); ++bit) {
    bset.insert(*bit);
  }

  // a stmt is a loop exit condition if it has outgoing edges to
  // blocks both inside and outside the loop.
  //
  for (auto bit = inclBlocks.begin(); bit != inclBlocks.end(); ++bit) {
    const Block::edgelist & outEdges = (*bit)->targets();
    VMA src_vma = (*bit)->last();
    bool in_loop = false, out_loop = false;

    for (auto eit = outEdges.begin(); eit != outEdges.end(); ++eit) {
      Block *dest = (*eit)->trg();

      if (bset.find(dest) != bset.end()) { in_loop = true; }
      else { out_loop = true; }
    }

    if (in_loop && out_loop) {
      clist[src_vma] = HeaderInfo(*bit);
      clist[src_vma].is_cond = true;
      clist[src_vma].score = 2;
    }
  }

  // add bonus points if the stmt is also a back edge source
  vector <Edge *> backEdges;
  loop->getBackEdges(backEdges);

  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    VMA src_vma = (*eit)->src()->last();

    auto it = clist.find(src_vma);
    if (it != clist.end()) {
      it->second.is_src = true;
      it->second.score += 1;
    }
  }

#if DEBUG_CFG_SOURCE
  cout << "\nraw inline tree:  " << loopName
       << "  '" << func->name() << "'\n"
       << "file:  '" << finfo->fileName << "'\n\n";
  debugInlineTree(root, NULL, strTab, 0, false);
  debugLoop(ginfo, func, loop, loopName, backEdges, clist);
  cout << "\nsearching inline tree:\n";
#endif

  //------------------------------------------------------------
  // Step 2 -- find the right inline depth
  //------------------------------------------------------------

  // start at the root, descend the inline tree and try to find the
  // right level for the loop.  an inline branch or subloop is an
  // absolute stopping point.  the hard case is one inline subtree
  // plus statements.  we stop if there is a loop condition, else
  // continue and reparent the stmts.  always reparent any stmt in a
  // different file from the inline callsite.

  FLPSeqn  path;
  StmtMap  stmts;

  while (root->nodeMap.size() == 1 && root->loopList.size() == 0) {
    FLPIndex flp = root->nodeMap.begin()->first;

    // look for loop cond at this level
    for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
      VMA vma = cit->first;

      if (cit->second.is_cond && root->stmtMap.member(vma)) {
	goto found_level;
      }

      // reparented stmts must also match file name
      StmtInfo * sinfo = stmts.findStmt(vma);
      if (cit->second.is_cond && sinfo != NULL && sinfo->base_index == flp.base_index) {
	goto found_level;
      }
    }

    // reparent the stmts and proceed to the next level
    for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
      stmts.insert(sit->second);
    }
    root->stmtMap.clear();

    TreeNode *subtree = root->nodeMap.begin()->second;
    root->nodeMap.clear();
    delete root;
    root = subtree;
    path.push_back(flp);
    base_index = -1;

    DEBUG_MESG("inline:  l=" << flp.line_num
	       << "  f='" << strTab.index2str(flp.file_index)
	       << "'  p='" << debugPrettyName(strTab.index2str(flp.proc_index))
	       << "'\n");
  }
found_level:

  //------------------------------------------------------------
  // Step 3 -- reattach stmts into this level
  //------------------------------------------------------------

  // fixme: want to attach some stmts below this level

  for (auto sit = stmts.begin(); sit != stmts.end(); ++sit) {
    root->stmtMap.insert(sit->second);
  }
  stmts.clear();

  //------------------------------------------------------------
  // Step 4 -- choose a loop header file/line at this level
  //------------------------------------------------------------

  long file_ans = 0;
  long base_ans = 0;
  long line_ans = 0;

  if (root->nodeMap.size() > 0 || root->loopList.size() > 0) {
    //
    // if there is an inline callsite or subloop, then use that file
    // name and the minimum line number among all callsites, subloops
    // and loop conditions with the same file.
    //
    // if there is an inconsistent choice of file name, prefer the
    // file matching the function, but do this only at top-level (no
    // inline steps).  inline call sites show only where called from,
    // not where defined.
    //
    for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
      FLPIndex flp = nit->first;
      file_ans = flp.file_index;
      base_ans = flp.base_index;
      line_ans = flp.line_num;

      if (base_index < 0 || flp.base_index == base_index) {
	goto found_file;
      }
    }

    for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
      LoopInfo *info = *lit;
      file_ans = info->file_index;
      base_ans = info->base_index;
      line_ans = info->line_num;

      if (base_index < 0 || info->base_index == base_index) {
	goto found_file;
      }
    }
found_file:

    // min of inline callsites
    for (auto nit = root->nodeMap.begin(); nit != root->nodeMap.end(); ++nit) {
      FLPIndex flp = nit->first;

      if (flp.base_index == base_ans && flp.line_num < line_ans) {
	line_ans = flp.line_num;
      }
    }

    // min of subloops
    for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
      LoopInfo *info = *lit;

      if (info->base_index == base_ans && info->line_num < line_ans) {
	line_ans = info->line_num;
      }
    }

    // min of loop cond stmts
    for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
      VMA vma = cit->first;
      StmtInfo *info = root->stmtMap.findStmt(vma);

      if (cit->second.is_cond && info != NULL && info->base_index == base_ans
	  && info->line_num < line_ans) {
	line_ans = info->line_num;
      }
    }
  }
  else {
    //
    // if there are only terminal stmts, then select the file name of
    // the best candidate (loop cond + back edge source, loop cond,
    // then any stmt) and then the min line number.
    //
    int max_score = -1;

    for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
      VMA vma = sit->first;
      StmtInfo *info = sit->second;
      auto it = clist.lower_bound(vma);
      int score = (it != clist.end() && info->member(it->first))
                   ? it->second.score : 0;

      if (score > max_score) {
	max_score = score;
	file_ans = info->file_index;
	base_ans = info->base_index;
	line_ans = info->line_num;
      }
      else if (score == max_score && info->base_index == base_ans
	       && info->line_num < line_ans) {
	line_ans = info->line_num;
      }
    }
  }

  DEBUG_MESG("header:  l=" << line_ans << "  f='"
	     << strTab.index2str(file_ans) << "'\n");

  vector <Block *> entryBlocks;
  loop->getLoopEntries(entryBlocks);
  VMA entry_vma = (*(entryBlocks.begin()))->start();

  LoopInfo *info = new LoopInfo(root, path, loopName, entry_vma,
				file_ans, base_ans, line_ans);

#if DEBUG_CFG_SOURCE
  cout << "\nreparented inline tree:  " << loopName
       << "  '" << func->name() << "'\n\n";
  debugInlineTree(root, info, strTab, 0, false);
#endif

  return info;
}

//----------------------------------------------------------------------

// Delete the inline sequence 'prefix' from root's tree and reparent
// any statements or loops.  We expect there to be no statements,
// loops or subtrees along the deleted spine, but if there are, move
// them to the subtree.
//
// Returns: the subtree at the end of prefix.
//
static TreeNode *
deleteInlinePrefix(TreeNode * root, Inline::InlineSeqn prefix, HPC::StringTable & strTab)
{
  StmtMap  stmts;
  LoopList loops;

  // walk the prefix and collect any stmts or loops
  for (auto pit = prefix.begin(); pit != prefix.end(); ++pit)
  {
    FLPIndex flp(strTab, *pit);
    auto nit = root->nodeMap.find(flp);

    if (nit != root->nodeMap.end()) {
      TreeNode * subtree = nit->second;

      // statements
      for (auto sit = root->stmtMap.begin(); sit != root->stmtMap.end(); ++sit) {
	stmts.insert(sit->second);
      }
      root->stmtMap.clear();

      // loops
      for (auto lit = root->loopList.begin(); lit != root->loopList.end(); ++lit) {
	loops.push_back(*lit);
      }
      root->loopList.clear();

      // subtrees
      for (auto it = root->nodeMap.begin(); it != root->nodeMap.end(); ++it) {
	TreeNode * node = it->second;
	if (node != subtree) {
	  mergeInlineEdge(subtree, it->first, node);
	}
      }
      root->nodeMap.clear();
      delete root;

      root = subtree;
    }
  }

  // reattach the stmts and loops
  for (auto sit = stmts.begin(); sit != stmts.end(); ++sit) {
    root->stmtMap.insert(sit->second);
  }
  stmts.clear();

  for (auto lit = loops.begin(); lit != loops.end(); ++lit) {
    root->loopList.push_back(*lit);
  }
  loops.clear();

  return root;
}

//----------------------------------------------------------------------

// Compute gaps = the set of intervals in [start, end) that are not
// covered in vset.
//
static void
computeGaps(VMAIntervalSet & vset, VMAIntervalSet & gaps, VMA start, VMA end)
{
  gaps.clear();

  auto it = vset.begin();

  while (start < end)
  {
    if (it == vset.end()) {
      gaps.insert(start, end);
      break;
    }
    else if (it->end() <= start) {
      // it entirely left of start
      ++it;
    }
    else if (it->beg() <= start) {
      // it contains start
      start = it->end();
      ++it;
    }
    else {
      // it entirely right of start
      VMA gap_end = std::min(it->beg(), end);
      gaps.insert(start, gap_end);
      start = it->end();
      ++it;
    }
  }
}

//****************************************************************************
// Debug functions
//****************************************************************************

#if DEBUG_CFG_SOURCE

// Debug functions to display the raw input data from ParseAPI for
// loops, blocks, stmts, file names, proc names and line numbers.

#define INDENT   "   "

// Cleanup and shorten the proc name: demangle plus collapse nested
// <...> and (...) to just <> and ().  For example:
//
//   std::map<int,long>::myfunc(int) --> std::map<>::myfunc()
//
// This is only for more compact debug output.  Internal decisions are
// always made on the full string.
//
static string
debugPrettyName(const string & procnm)
{
  string str = BinUtil::demangleProcName(procnm);
  string ans = "";
  size_t str_len = str.size();
  size_t pos = 0;

  while (pos < str_len) {
    size_t next = str.find_first_of("<(", pos);
    char open, close;

    if (next == string::npos) {
      ans += str.substr(pos);
      break;
    }
    if (str[next] == '<') { open = '<';  close = '>'; }
    else { open = '(';  close = ')'; }

    ans += str.substr(pos, next - pos) + open + close;

    int depth = 1;
    for (pos = next + 1; pos < str_len && depth > 0; pos++) {
      if (str[pos] == open) { depth++; }
      else if (str[pos] == close) { depth--; }
    }
  }

  return ans;
}

//----------------------------------------------------------------------

static void
debugStmt(VMA vma, int len, string & filenm, SrcFile::ln line)
{
  cout << INDENT << "stmt:  0x" << hex << vma << dec << " (" << len << ")"
       << "  l=" << line << "  f='" << filenm << "'\n";

  Inline::InlineSeqn nodeList;
  Inline::analyzeAddr(nodeList, vma);

  // list is outermost to innermost
  for (auto nit = nodeList.begin(); nit != nodeList.end(); ++nit) {
    cout << INDENT << INDENT << "inline:  l=" << nit->getLineNum()
	 << "  f='" << nit->getFileName()
	 << "'  p='" << debugPrettyName(nit->getProcName()) << "'\n";
  }
}

//----------------------------------------------------------------------

static void
debugLoop(GroupInfo * ginfo, ParseAPI::Function * func,
	  Loop * loop, const string & loopName,
	  vector <Edge *> & backEdges, HeaderList & clist)
{
  vector <Block *> entBlocks;
  int num_ents = loop->getLoopEntries(entBlocks);

  cout << "\nheader info:  " << loopName
       << ((num_ents == 1) ? "  (reducible)" : "  (irreducible)")
       << "  '" << func->name() << "'\n\n";

  cout << "entry blocks:" << hex;
  for (auto bit = entBlocks.begin(); bit != entBlocks.end(); ++bit) {
    cout << "  0x" << (*bit)->start();
  }

  cout << "\nback edge sources:";
  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    cout << "  0x" << (*eit)->src()->last();
  }

  cout << "\nback edge targets:";
  for (auto eit = backEdges.begin(); eit != backEdges.end(); ++eit) {
    cout << "  0x" << (*eit)->trg()->start();
  }

  cout << "\n\nexit conditions:\n";
  for (auto cit = clist.begin(); cit != clist.end(); ++cit) {
    VMA vma = cit->first;
    HeaderInfo * info = &(cit->second);
    SrcFile::ln line;
    string filenm, label;

    InlineSeqn seqn;
    analyzeAddr(seqn, vma);

    StatementVector svec;
    getStatement(svec, vma, ginfo->sym_func);

    if (! svec.empty()) {
      filenm = svec[0]->getFile();
      line = svec[0]->getLine();
    }

    if (info->is_src) { label = "src "; }
    else if (info->is_targ) { label = "targ"; }
    else if (info->is_cond) { label = "cond"; }
    else { label = "??? "; }

    cout << "0x" << hex << vma << dec
	 << "  " << label
	 << "  excl: " << loop->hasBlockExclusive(info->block)
	 << "  cond: " << info->is_cond
	 << "  depth: " << seqn.size()
	 << "  l=" << line
	 << "  f='" << filenm << "'\n";
  }
}

//----------------------------------------------------------------------

// If LoopInfo is non-null, then treat 'node' as a detached loop and
// prepend the FLP seqn from 'info' above the tree.  Else, 'node' is
// the tree.
//
static void
debugInlineTree(TreeNode * node, LoopInfo * info, HPC::StringTable & strTab,
		int depth, bool expand_loops)
{
  // treat node as a detached loop with FLP seqn above it.
  if (info != NULL) {
    depth = 0;
    for (auto pit = info->path.begin(); pit != info->path.end(); ++pit) {
      for (int i = 1; i <= depth; i++) {
	cout << INDENT;
      }
      FLPIndex flp = *pit;

      cout << "inline:  l=" << flp.line_num
	   << "  f='" << strTab.index2str(flp.file_index)
	   << "'  p='" << debugPrettyName(strTab.index2str(flp.proc_index))
	   << "'\n";
      depth++;
    }

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "loop:  " << info->name
	 << "  l=" << info->line_num
	 << "  f='" << strTab.index2str(info->file_index) << "'\n";
    depth++;
  }

  // print the terminal statements
  for (auto sit = node->stmtMap.begin(); sit != node->stmtMap.end(); ++sit) {
    StmtInfo *sinfo = sit->second;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "stmt:  0x" << hex << sinfo->vma << dec << " (" << sinfo->len << ")"
	 << "  l=" << sinfo->line_num
	 << "  f='" << strTab.index2str(sinfo->file_index) << "'\n";
  }

  // recur on the subtrees
  for (auto nit = node->nodeMap.begin(); nit != node->nodeMap.end(); ++nit) {
    FLPIndex flp = nit->first;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }
    cout << "inline:  l=" << flp.line_num
	 << "  f='"  << strTab.index2str(flp.file_index)
	 << "'  p='" << debugPrettyName(strTab.index2str(flp.proc_index))
	 << "'\n";

    debugInlineTree(nit->second, NULL, strTab, depth + 1, expand_loops);
  }

  // recur on the loops
  for (auto lit = node->loopList.begin(); lit != node->loopList.end(); ++lit) {
    LoopInfo *info = *lit;

    for (int i = 1; i <= depth; i++) {
      cout << INDENT;
    }

    cout << "loop:  " << info->name
	 << "  l=" << info->line_num
	 << "  f='" << strTab.index2str(info->file_index) << "'\n";

    if (expand_loops) {
      debugInlineTree(info->node, NULL, strTab, depth + 1, expand_loops);
    }
  }
}

#endif  // DEBUG_CFG_SOURCE

}  // namespace Struct
}  // namespace BAnal
