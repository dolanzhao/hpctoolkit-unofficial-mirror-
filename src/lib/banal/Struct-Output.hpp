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
// Copyright ((c)) 2002-2023, Rice University
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

// This file defines the API for writing an hpcstruct file directly
// from the TreeNode format.

//***************************************************************************

#ifndef Banal_Struct_Output_hpp
#define Banal_Struct_Output_hpp

#include <ostream>
#include <string>

#include <lib/support/StringTable.hpp>

#include "Struct-Inline.hpp"
#include "Struct-Skel.hpp"

namespace BAnal {
namespace Output {

using namespace Struct;
using namespace std;

void printStructFileBegin(ostream *, ostream *, string);
void printStructFileEnd(ostream *, ostream *);

void printLoadModuleBegin(ostream *, string, bool has_calls);
void printLoadModuleEnd(ostream *);

void printFileBegin(ostream *, FileInfo *);
void printFileEnd(ostream *, FileInfo *);

void earlyFormatProc(ostream *, FileInfo *, GroupInfo *, ProcInfo *,
                     bool, HPC::StringTable & strTab);

void finalPrintProc(ostream *, ostream *, string &, string &,
                    FileInfo *, GroupInfo *, ProcInfo *);

void setPrettyPrint(bool _pretty_print_output);

void enableCallTags();

}  // namespace Output
}  // namespace BAnal

#endif
