#include "./printer.hpp"

#include <unordered_set>
#include <vector>

namespace clam {
namespace crab_pretty_printer {

invariant_annotation::invariant_annotation(
    const abs_dom_map_t &premap, const abs_dom_map_t &postmap,
    const std::vector<varname_t> &shadow_vars,
    invariant_annotation::lookup_function lookup)
    : block_annotation(), m_premap(premap), m_postmap(postmap),
      m_shadow_vars(shadow_vars), m_lookup(lookup) {}

void invariant_annotation::print_begin(basic_block_label_t bbl,
                                       crab::crab_os &o) const {
  if (const llvm::BasicBlock *bb = bbl.get_basic_block()) {
    auto pre = m_lookup(m_premap, *bb, m_shadow_vars);
    o << "  " << name() << ": ";
    if (pre.hasValue()) {
      o << pre.getValue() << "\n";
    } else {
      o << "null\n";
    }
  }
}

void invariant_annotation::print_end(basic_block_label_t bbl,
                                     crab::crab_os &o) const {
  if (const llvm::BasicBlock *bb = bbl.get_basic_block()) {
    auto post = m_lookup(m_postmap, *bb, m_shadow_vars);
    o << "  " << name() << ": ";
    if (post.hasValue()) {
      o << post.getValue() << "\n";
    } else {
      o << "null\n";
    }
  }
}

unproven_assumption_annotation::unproven_assumption_annotation(
    cfg_ref_t cfg, unproven_assumption_analysis_t *analyzer)
    : block_annotation(), m_cfg(cfg), m_analyzer(analyzer) {}

void unproven_assumption_annotation::print_begin(const statement_t &s,
                                                 crab::crab_os &o) const {
  std::vector<assumption_ptr> assumes;
  if (s.is_assert()) {
    typedef typename cfg_ref_t::basic_block_t::assert_t assert_t;
    m_analyzer->get_assumptions(static_cast<const assert_t *>(&s), assumes);
    if (!assumes.empty()) {
      o << "  /** assert verified as ";
      for (std::vector<assumption_ptr>::iterator it = assumes.begin(),
                                                 et = assumes.end();
           it != et;) {
        o << (*it)->get_id_str();
        ++it;
        if (it != et)
          o << ",";
        else
          o << ";";
      }
      o << "**/\n";
    }
  } else {
    m_analyzer->get_originated_assumptions(&s, assumes);
    for (auto assume_ptr : assumes) {
      o << "  /** ";
      assume_ptr->write(o);
      o << "**/\n";
    }
  }
}

print_block::print_block(
    cfg_ref_t cfg, crab::crab_os &o,
    const typename IntraClam::checks_db_t &checksdb,
    const std::vector<std::unique_ptr<block_annotation>> &annotations)
    : m_cfg(cfg), m_o(o), m_checksdb(checksdb), m_annotations(annotations) {}

void print_block::operator()(basic_block_label_t bbl) const {
  // do not print block if no annotations
  if (m_annotations.empty())
    return;

  m_o << bbl.get_name() << ":\n";

  crab::crab_string_os o;
  for (auto &p : m_annotations) {
    p->print_begin(bbl, o);
  }
  if (o.str() != "") {
    m_o << "/**\n" << o.str() << "**/\n";
  }

  const basic_block_t &bb = m_cfg.get_node(bbl);
  bool empty_block = (std::distance(bb.begin(), bb.end()) == 0);
  for (auto const &s : bb) {
    for (auto &p : m_annotations) {
      p->print_begin(s, m_o);
    }

    if (s.is_assert() || s.is_ref_assert() || s.is_bool_assert()) {
      const crab::cfg::debug_info &di = s.get_debug_info();
      if (m_checksdb.has_checks(di)) {
        m_o << "  // File:" << di.get_file() << " line:" << di.get_line()
            << " col:" << di.get_column() << " ";
        m_o << "Result: ";
        auto const &checks = m_checksdb.get_checks(di);
        unsigned safe = 0;
        unsigned warning = 0;
        unsigned error = 0;
        for (unsigned i = 0, num_checks = checks.size(); i < num_checks; ++i) {
          switch (checks[i]) {
          case crab::checker::_SAFE:
          case crab::checker::_UNREACH:
            safe++;
            break;
          case crab::checker::_ERR:
            error++;
            break;
          default:
            warning++;
            break;
          }
        }
        if (error == 0 && warning == 0) {
          m_o << " OK";
        } else {
          m_o << " FAIL -- ";
          if (safe > 0)
            m_o << "num of safe=" << safe << " ";
          if (error > 0)
            m_o << "num of errors=" << error << " ";
          if (warning > 0)
            m_o << "num of warnings=" << warning << " ";
        }
        m_o << "\n";
      }
    }

    m_o << "  " << s << ";\n";

    for (auto &p : m_annotations) {
      p->print_end(s, m_o);
    }
  }
  if (!empty_block) {
    crab::crab_string_os o;
    for (auto &p : m_annotations) {
      p->print_end(bbl, o);
    }
    if (o.str() != "") {
      m_o << "/**\n" << o.str() << "**/\n";
    }
  }

  std::pair<cfg_ref_t::const_succ_iterator, cfg_ref_t::const_succ_iterator> p =
      bb.next_blocks();
  cfg_ref_t::const_succ_iterator it = p.first;
  cfg_ref_t::const_succ_iterator et = p.second;
  if (it != et) {
    m_o << "  "
        << "goto ";
    for (; it != et;) {
      m_o << crab::basic_block_traits<basic_block_t>::to_string(*it);
      ++it;
      if (it == et) {
        m_o << ";";
      } else {
        m_o << ",";
      }
    }
  }
  m_o << "\n";
}

using visited_t = std::unordered_set<basic_block_label_t>;
template <typename T>
void dfs_rec(cfg_ref_t cfg, basic_block_label_t curId, visited_t &visited,
             T f) {
  if (visited.find(curId) != visited.end())
    return;
  visited.insert(curId);
  const basic_block_t &cur = cfg.get_node(curId);
  f(curId);
  for (auto const n : llvm::make_range(cur.next_blocks())) {
    dfs_rec(cfg, n, visited, f);
  }
}

template <typename T> void dfs(cfg_ref_t cfg, T f) {
  visited_t visited;
  dfs_rec(cfg, cfg.entry(), visited, f);
}

void print_annotations(
    cfg_ref_t cfg, const typename IntraClam::checks_db_t &checksdb,
    const std::vector<std::unique_ptr<block_annotation>> &annotations) {
  print_block f(cfg, crab::outs(), checksdb, annotations);
  dfs(cfg, f);
}

} // namespace crab_pretty_printer
} // namespace clam
