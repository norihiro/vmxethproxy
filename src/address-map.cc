#include <cstdio>
#include <algorithm>
#include "address-map.h"

namespace address_map {

static constexpr char SEP = '/';

static void load_list(element_tree *root, const data_t &data);
static void load_struct_loop(element_tree *root, const data_t &data);
static uint32_t load_struct(element_tree *parent, const char *name, const char *type_name, uint32_t address_offset);

static uint32_t str_to_packed(const char *str)
{
	uint32_t ret = 0;
	while (str && *str) {
		int val = strtol(str, (char **)&str, 16);
		ret = (ret << 7) | (val & 0x7F);
		while (str && *str == ' ')
			str++;
	}
	return ret;
}

static uint32_t load_packed_address(const data_t &data, const char *name, uint32_t def = 0)
{
	boost::optional<uint32_t> int_val = data.get_optional<uint32_t>(name);
	if (int_val)
		return *int_val;

	boost::optional<std::string> str_val = data.get_optional<std::string>(name);
	if (str_val)
		return str_to_packed(str_val->c_str());

	return def;
}

void element_tree::set_name(const char *parent_path, const char *name)
{
	if (parent_path && *parent_path)
		abs_path = parent_path;
	else
		abs_path = "";

	if (name && *name) {
		if (abs_path.size() && name && *name)
			abs_path += SEP;
		abs_path += name;
		this->name = name;
	}
}

static element_tree *load_address_map_with_parent(const data_t &data, element_tree *parent)
{
	element_tree *root = new element_tree();
	root->parent = parent;

	boost::optional<std::string> name = data.get_optional<std::string>("name");
	root->set_name(parent ? parent->abs_path.c_str() : nullptr, name ? name->c_str() : nullptr);

	root->address_begin = load_packed_address(data, "address") + parent->address_begin;
	uint32_t size = load_packed_address(data, "size", 1);
	root->address_end = size + root->address_begin;

	std::string type_s = data.get<std::string>("type");
	if (type_s == "bool") {
		root->leaf.type = type_bool;
		root->leaf.is_signed = false;
		if (size != 1) {
			fprintf(stderr, "Error: %s: size for bool must be 1 but got %u\n", root->abs_path.c_str(),
				size);
			throw "size for bool must be 1";
		}
	}
	else if (type_s == "linear-float") {
		root->leaf.type = type_linear_float;
		root->leaf.is_signed = true;
		root->leaf.min_code = data.get<int32_t>("min_code");
		root->leaf.max_code = data.get<int32_t>("max_code");
		root->leaf.min_value = data.get<float>("min_value");
		root->leaf.max_value = data.get<float>("max_value");
	}
	else if (type_s == "list-string") {
		root->leaf.type = type_list_string;
		root->leaf.is_signed = false;

		for (auto &it : data.get_child("list")) {
			int32_t val = strtol(it.first.c_str(), NULL, 0);
			root->leaf.string_list[val] = it.second.get<std::string>("");
		}
	}
	else if (type_s == "string") {
		root->leaf.type = type_string;
		root->leaf.is_signed = false;
	}
	else if (type_s == "struct-loop") {
		load_struct_loop(root, data);
	}
	else {
		fprintf(stderr, "Error: %s: unknown type '%s'\n", root->abs_path.c_str(), type_s.c_str());
		throw "unknown type";
	}

	return root;
}

static element_tree *load_struct_def(const data_t &data, element_tree *parent)
{
	element_tree *root = new element_tree();
	root->parent = parent;

	root->address_begin = 0;
	root->address_end = load_packed_address(data, "size");

	boost::optional<const data_t &> contents = data.get_child_optional("contents");
	if (contents)
		load_list(root, *contents);

	return root;
}

static const element_tree *find_defined_type(const element_tree *parent, const char *type_name)
{
	const auto &it = parent->struct_defs.find(type_name);
	if (it != parent->struct_defs.end())
		return it->second.get();

	if (parent->parent)
		return find_defined_type(parent->parent, type_name);

	return nullptr;
}

static void copy_element(element_tree &dst, const element_tree &src)
{
	dst.leaf = src.leaf;

	for (const auto &c : src.children) {
		std::unique_ptr<element_tree> e(new element_tree());
		e->parent = &dst;
		e->set_name(dst.abs_path.c_str(), c->name.c_str());
		e->address_begin = dst.address_begin + c->address_begin - src.address_begin;
		e->address_end = dst.address_begin + c->address_end - src.address_begin;
		copy_element(*e, *c);
		dst.children.push_back(std::move(e));
	}
}

static uint32_t load_struct(element_tree *parent, const char *name, const char *type_name, uint32_t address_offset)
{
	const element_tree *t = find_defined_type(parent, type_name);
	if (!t) {
		fprintf(stderr, "Error: %s: cannot load definition '%s'\n", parent->abs_path.c_str(), type_name);
		return 0;
	}

	std::unique_ptr<element_tree> e(new element_tree());
	e->parent = parent;
	e->address_begin = parent->address_begin + address_offset;
	e->address_end = e->address_begin + t->address_end;
	e->set_name(parent->abs_path.c_str(), name);
	copy_element(*e, *t);

	auto end = e->address_end - e->address_begin;
	parent->children.push_back(std::move(e));

	return end;
}

static void load_struct_loop(element_tree *root, const data_t &data)
{
	auto n_elements = data.get<uint32_t>("n_elements");
	auto element_id_start = data.get<int32_t>("element_id_start");
	auto element_type = data.get<std::string>("element_type");

	uint32_t address_offset = 0;
	for (uint32_t i = 0; i < n_elements; i++) {
		char name[64] = {0};
		snprintf(name, sizeof(name) - 1, "%d", element_id_start + i);
		address_offset += load_struct(root, name, element_type.c_str(), address_offset);
	}

	root->address_end = root->address_begin + address_offset;
}

static void load_list(element_tree *root, const data_t &data)
{
	for (auto &it : data) {
		const data_t &element_data = it.second;
		std::string type_s = element_data.get<std::string>("type");
		if (type_s == "struct-definition") {
			std::string name = element_data.get<std::string>("name");
			root->struct_defs[name].reset(load_struct_def(element_data, root));
		}
		else {
			root->children.emplace_back(load_address_map_with_parent(element_data, root));
		}
	}

	for (const auto &c : root->children)
		root->address_end = std::max(root->address_end, c->address_end);
}

element_tree *load_address_map(const data_t &data, const char *prefix)
{
	const data_t &addresses = data.get_child("addresses");

	element_tree *root = new element_tree();
	root->set_name(prefix, nullptr);
	load_list(root, addresses);

	return root;
}

} // namespace address_map
