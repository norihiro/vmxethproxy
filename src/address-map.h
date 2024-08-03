#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <memory>
#include <boost/property_tree/ptree.hpp>

namespace address_map {

typedef boost::property_tree::ptree data_t;

enum type_t {
	type_none = 0,
	type_bool,
	type_linear_float,
	type_list_string,
	type_string,
	/* TODO: How about `type_pwl` piecewise linear to define the fader position */
};

struct leaf_s
{
	type_t type = type_none;
	bool is_signed;

	int32_t current_value;
	std::string current_string;

	/* for type_linear_float */
	int32_t min_code = 0, max_code = 0;
	float min_value = 0, max_value = 0;

	/* for type_list_string */
	std::map<int32_t, std::string> string_list;
};

struct element_tree
{
	std::string name;
	std::string abs_path;

	uint32_t address_begin;
	uint32_t address_end;

	struct leaf_s leaf;

	element_tree *parent = nullptr;
	std::vector<std::unique_ptr<element_tree>> children;
	std::map<std::string, std::unique_ptr<element_tree>> struct_defs;

	void set_name(const char *parent_path, const char *name);
};

element_tree *load_address_map(const data_t &data, const char *prefix);

} // namespace address_map
