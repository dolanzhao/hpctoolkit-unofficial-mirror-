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

#include "../util/vgannotations.hpp"

#include "packed.hpp"

#include <limits>
#include <stack>

using namespace hpctoolkit;
using namespace sinks;

// Helpers for packing various things.
static void pack(std::vector<std::uint8_t>& out, const std::string& s) noexcept {
  out.reserve(out.size() + s.size() + 1);  // Allocate the space early
  for(auto c: s) {
    if(c == '\0') c = '?';
    out.push_back(c);
  }
  out.push_back('\0');
}
static void pack(std::vector<std::uint8_t>& out, const std::uint8_t v) noexcept {
  out.push_back(v);
}
static std::uint8_t* pack(std::uint8_t* out, const std::uint8_t v) noexcept {
  *out = v;
  return ++out;
}
static void pack(std::vector<std::uint8_t>& out, const std::uint16_t v) noexcept {
  // Little-endian order. Just in case the compiler can optimize it away.
  for(int shift = 0; shift < 16; shift += 8)
    out.push_back((v >> shift) & 0xff);
}
static void pack(std::vector<std::uint8_t>& out, const std::uint64_t v) noexcept {
  // Little-endian order. Just in case the compiler can optimize it away.
  for(int shift = 0; shift < 64; shift += 8)
    out.push_back((v >> shift) & 0xff);
}
static std::uint8_t* pack(std::uint8_t* out, const std::uint64_t v) noexcept {
  // Little-endian order. Just in case the compiler can optimize it away.
  for(int shift = 0x00; shift < 0x40; shift += 0x08)
    *(out++) = (v >> shift) & 0xff;
  return out;
}
static void pack(std::vector<std::uint8_t>& out, const double v) noexcept {
  // Assumes doubles are compatible across systems
  union { double d; std::uint64_t u; } x;
  x.d = v;
  pack(out, x.u);
}
static std::uint8_t* pack(std::uint8_t* out, const double v) noexcept {
  // Assumes doubles are compatible across systems
  union { double d; std::uint64_t u; } x;
  x.d = v;
  return pack(out, x.u);
}
static_assert(std::is_same_v<StatisticAccumulator::raw_t, std::array<double, 5>>, "pack needs to be updated!");
static const inline size_t raw_size = 5 * 8;
static void pack(std::vector<std::uint8_t>& out, const StatisticAccumulator::raw_t& v) noexcept {
  pack(out, v[0]);
  pack(out, v[1]);
  pack(out, v[2]);
  pack(out, v[3]);
  pack(out, v[4]);
}
static std::uint8_t* pack(std::uint8_t* out, const StatisticAccumulator::raw_t& v) noexcept {
  out = pack(out, v[0]);
  out = pack(out, v[1]);
  out = pack(out, v[2]);
  out = pack(out, v[3]);
  return pack(out, v[4]);
}

Packed::Packed()
  : metrics(), moduleIDs() {};

void Packed::packAttributes(std::vector<std::uint8_t>& out) noexcept {
  // Format: [job or magic] [name] [path] [env cnt] ([env key] [env val]...)
  const auto& attr = src.attributes();
  pack(out, (std::uint64_t)attr.job().value_or(0xFEF1F0F3ULL << 32));
  pack(out, attr.name().value_or(""));
  pack(out, attr.path() ? attr.path()->string() : "");
  pack(out, (std::uint64_t)attr.environment().size());
  for(const auto& kv: attr.environment()) {
    pack(out, kv.first);
    pack(out, kv.second);
  }
  pack(out, (std::uint64_t)attr.idtupleNames().size());
  for(const auto& kv: attr.idtupleNames()) {
    pack(out, (std::uint16_t)kv.first);
    pack(out, kv.second);
  }

  // Format: [cnt] ([metric name] [metric description] [scopes] [visibility] [orderId] [num partials])...
  metrics.clear();
  pack(out, src.metrics().size());
  std::unordered_map<const Metric*, size_t> metids;
  for(const Metric& m: src.metrics().citerate()) {
    metids.emplace(&m, metrics.size());
    metrics.emplace_back(m);
    pack(out, m.name());
    pack(out, m.description());
    pack(out, m.scopes().toInt());
    pack(out, (std::uint8_t)m.visibility());
    if(auto o = m.orderId()) {
      pack(out, (std::uint64_t)*o);
    } else {
      pack(out, std::numeric_limits<std::uint64_t>::max());
    }
    pack(out, (std::uint8_t)m.partials().size());
  }

  // Format: [cnt] ([estat name] [estat description] [scopes] [formula])...
  // Where [formula] = [kind] ([arg cnt] if op) [formula args]...
  pack(out, src.extraStatistics().size());
  for(const ExtraStatistic& es: src.extraStatistics().citerate()) {
    pack(out, es.name());
    pack(out, es.description());
    pack(out, es.scopes().toInt());
    es.formula().citerate_all(
      [&](double value){
        pack(out, (std::uint8_t)Expression::Kind::constant);
        pack(out, value);
      },
      [&](Expression::uservalue_t var){
        pack(out, (std::uint8_t)Expression::Kind::variable);
        pack(out, (std::uint64_t)metids.at((const Metric*)var));
      },
      [&](const Expression& ex){
        pack(out, (std::uint8_t)ex.kind());
        pack(out, (std::uint8_t)ex.op_args().size());
      },
      nullptr);
  }
}

void Packed::packReferences(std::vector<std::uint8_t>& out) noexcept {
  // Format: [cnt] (([module path] [module relative path])...)
  moduleIDs.clear();
  pack(out, src.modules().size());
  for(const Module& m: src.modules().citerate()) {
    moduleIDs.emplace(&m, moduleIDs.size());
    pack(out, m.path().string());
    pack(out, m.relative_path().string());
  }
}

void Packed::packContexts(std::vector<std::uint8_t>& out) noexcept {
  // Format: [Scope.type] [Scope.data...] children... [sentinel]
  src.contexts().citerate([&](const Context& c){
    if(c.scope().relation() != Relation::global
       && c.scope().relation() != Relation::call) {
      util::log::fatal{} << "sinks::Packed does not support non-physical calling Context!";
    }
    Scope s = c.scope().flat();
    pack(out, (std::uint64_t)s.type());
    switch(s.type()) {
    case Scope::Type::point: {
      // Format: [module id] [offset]
      auto mo = c.scope().flat().point_data();
      pack(out, moduleIDs.at(&mo.first));
      pack(out, mo.second);
      break;
    }
    case Scope::Type::placeholder:
      // Format: [placeholder]
      pack(out, (std::uint64_t)s.enumerated_data());
      break;
    case Scope::Type::unknown:
    case Scope::Type::global:
    case Scope::Type::function:
    case Scope::Type::lexical_loop:
    case Scope::Type::binary_loop:
    case Scope::Type::line:
      // No data to represent, or unable to represent
      break;
    }
  }, [&](const Context& c){
    pack(out, (std::uint64_t)0xFEF1F0F3ULL << 32);
  });

  // Format: [cnt] ([Scope.type] Scope...)...
  pack(out, (std::uint64_t)src.contextFlowGraphs().size());
  for(const ContextFlowGraph& cg: src.contextFlowGraphs().citerate()) {
    auto s = cg.scope();
    pack(out, (std::uint64_t)s.type());
    switch(s.type()) {
    case Scope::Type::point: {
      // Format: [Scope.type] [module id] [offset]
      auto mo = s.point_data();
      pack(out, moduleIDs.at(&mo.first));
      pack(out, mo.second);
      break;
    }
    case Scope::Type::placeholder:
    case Scope::Type::unknown:
    case Scope::Type::global:
    case Scope::Type::function:
    case Scope::Type::lexical_loop:
    case Scope::Type::binary_loop:
    case Scope::Type::line:
      assert(false && "Unhandled Scope type in Packed!");
      std::abort();
    }
  }
}

void Packed::packMetrics(std::vector<std::uint8_t>& out) noexcept {
  // Format: [cnt] ([context ID] [cnt] ([metric ID] [use] [values]...)...)...
  auto start = out.size();
  std::uint64_t cnt = 0;
  pack(out, cnt);
  src.contexts().citerate([&](const Context& c){
    pack(out, (std::uint64_t)c.userdata[src.identifier()]);
    pack(out, (std::uint64_t)src.metrics().size());
    for(const Metric& m: src.metrics().citerate()) {
      pack(out, (std::uint64_t)m.userdata[src.identifier()]);
      pack(out, c.data().metricUsageFor(m).toInt());

      for(const auto& p: m.partials()) {
        if(auto v = m.getFor(c)) {
          pack(out, v->get(p).getRaw());
        } else {
          pack(out, StatisticAccumulator::rawZero());
        }
      }
    }
    cnt++;
  }, nullptr);
  // Skip back and overwrite the beginning.
  std::vector<std::uint8_t> tmp;
  pack(tmp, cnt);
  for(const auto b: tmp) out[start++] = b;
}

void Packed::packTimepoints(std::vector<std::uint8_t>& out) noexcept {
  auto [min, max] = src.timepointBounds().value_or(std::make_pair(
      std::chrono::nanoseconds::zero(), std::chrono::nanoseconds::zero()));
  pack(out, (std::uint64_t)min.count());
  pack(out, (std::uint64_t)max.count());
}

ParallelPacked::ParallelPacked(bool doContexts, bool doMetrics)
    : doContexts(doContexts), doMetrics(doMetrics), ctxCnt(0) {}

void ParallelPacked::notifyPipeline() noexcept {
  if(doContexts || doMetrics) {
    groupLocks = std::vector<std::mutex>(src.teamSize() * 5);
    if(doMetrics) packMetricsGroups = decltype(packMetricsGroups)(groupLocks.size());
  }
}

void ParallelPacked::notifyContext(const Context& ctx) {
  if(doContexts || doMetrics) {
    auto idx = ctxCnt.fetch_add(1, std::memory_order_relaxed) % groupLocks.size();
    std::unique_lock<std::mutex> l(groupLocks[idx]);
    if(doMetrics) packMetricsGroups[idx].push_back(ctx);
  }
}

void ParallelPacked::packAttributes(std::vector<std::uint8_t>& out) noexcept {
  Packed::packAttributes(out);
}

void ParallelPacked::packMetrics(std::vector<std::uint8_t>& out) noexcept {
  assert(doMetrics && "packMetrics is invalid if doMetrics was false!");
  assert(!packMetricsGroups.empty() && "packMetrics can only be called once!");
  std::size_t cCnt = ctxCnt.load(std::memory_order_relaxed);
  pack(out, (std::uint64_t)cCnt);

  bytesPerCtx = 8 + 8;
  for(const Metric& m: src.metrics().citerate())
    bytesPerCtx += 8 + sizeof(MetricScopeSet::int_type) + m.partials().size() * raw_size;

  // Its dense, so we know the exact size already. Allocate the space we need
  auto startIdx = out.size();
  out.reserve(out.size() + cCnt * bytesPerCtx);
  out.insert(out.end(), cCnt * bytesPerCtx, 0);
  output = &out[startIdx];

  // Stitch together the workitems from the bits we've collected previously
  std::vector<std::pair<std::size_t, std::vector<std::reference_wrapper<const Context>>>> workitems;
  workitems.reserve(packMetricsGroups.size());
  std::size_t prev = 0;
  for(auto& g: packMetricsGroups) {
    auto sz = g.size();
    workitems.emplace_back(prev, std::move(g));
    prev += sz;
  }

  fePackMetrics.fill(std::move(workitems),
                     [this](auto& group){ packMetricGroup(group); });
  packMetricsGroups.clear();
  fePackMetrics.contributeUntilComplete();
  output = nullptr;
}

util::WorkshareResult ParallelPacked::helpPackMetrics() noexcept {
  return fePackMetrics.contributeWhileAble();
}

void ParallelPacked::packMetricGroup(std::pair<std::size_t, std::vector<std::reference_wrapper<const Context>>>& task) noexcept {
  // Format: [cnt] ([context ID] [cnt] ([metric ID] [use] [values]...)...)...
  std::uint8_t* out = output + task.first * bytesPerCtx;
  for(const Context& c: std::move(task.second)) {
    out = pack(out, (std::uint64_t)c.userdata[src.identifier()]);
    out = pack(out, (std::uint64_t)src.metrics().size());
    for(const Metric& m: src.metrics().citerate()) {
      out = pack(out, (std::uint64_t)m.userdata[src.identifier()]);
      out = pack(out, c.data().metricUsageFor(m).toInt());

      for(const auto& p: m.partials()) {
        if(auto v = m.getFor(c)) {
          out = pack(out, v->get(p).getRaw());
        } else {
          out = pack(out, StatisticAccumulator::rawZero());
        }
      }
    }
  }
}
