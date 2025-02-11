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
// Copyright ((c)) 2002-2024, Rice University
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

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef Analysis_Raw_Raw_hpp
#define Analysis_Raw_Raw_hpp

//************************* System Include Files ****************************

#include <string>

//*************************** User Include Files ****************************

#include "../prof-lean/hpcrun-fmt.h"
#include "../prof-lean/id-tuple.h"

//*************************** Forward Declarations ***************************

//****************************************************************************

namespace Analysis {

namespace Raw {

void
writeAsText(/*destination,*/ const char* filenm, bool sm_easyToGrep);
//YUMENG: second arg: if more flags, maybe build a struct to include all flags and pass the struct around

void
writeAsText_callpath(/*destination,*/ const char* filenm, bool sm_easyToGrep);

void
writeAsText_profiledb(const char* filenm, bool sm_easyToGrep);

void
writeAsText_cctdb(const char* filenm, bool sm_easyToGrep);

void
writeAsText_tracedb(const char* filenm);

void
writeAsText_metadb(const char* filenm);

void
writeAsText_callpathMetricDB(/*destination,*/ const char* filenm);

void
writeAsText_callpathTrace(/*destination,*/ const char* filenm);

} // namespace Raw

} // namespace Analysis

//****************************************************************************

#endif // Analysis_Raw_Raw_hpp
