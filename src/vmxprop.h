#pragma once

#ifdef __cplusplus

#include <boost/property_tree/ptree.hpp>

typedef boost::property_tree::ptree vmx_prop_t;
typedef vmx_prop_t &vmx_prop_ref_t;

bool load_ptree(vmx_prop_t &pt, const char *fname);

#else  // !__cplusplus
typedef void *vmx_prop_ref_t;
#endif // __cplusplus
