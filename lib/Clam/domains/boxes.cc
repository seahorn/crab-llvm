#include "clam/crab/domains/boxes.hh"
#include "clam/RegisterAnalysis.hh"
#include "clam/config.h"
namespace clam {
#ifdef HAVE_LDD
template <>
clam_abstract_domain DomainRegistry::makeTopDomain<boxes_domain_t>() {
  boxes_domain_t dom_val;
  clam_abstract_domain res(std::move(dom_val));
  return res;
}
#endif
} // end namespace clam