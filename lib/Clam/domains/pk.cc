#include "clam/crab/domains/pk.hh"
#include "clam/RegisterAnalysis.hh"
#include "clam/config.h"
namespace clam {
#if defined(HAVE_APRON) || defined(HAVE_ELINA)
template <> clam_abstract_domain DomainRegistry::makeTopDomain<pk_domain_t>() {
  pk_domain_t dom_val;
  clam_abstract_domain res(std::move(dom_val));
  return res;
}
#endif
} // end namespace