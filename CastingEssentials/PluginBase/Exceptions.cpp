#include "PluginBase/Exceptions.h"

#include <sstream>

invalid_class_prop::invalid_class_prop(const char *name) noexcept
{
	className = name;
}

const char *invalid_class_prop::what() const noexcept
{
	std::string s;
	std::stringstream ss;

	ss << "Attempted to access invalid property in entity class " << className << "!\n";
	ss >> s;

	return s.c_str();
}

mismatching_entity_offset::mismatching_entity_offset(const char* expected, const char* actual) noexcept
	: std::runtime_error(BuildMessage(expected, actual))
{
}

std::string mismatching_entity_offset::BuildMessage(const char* expected, const char* actual)
{
	std::stringstream ss;
	ss << "Attempted to use an EntityOffset intended for " << expected << " on " << actual;
	return ss.str();
}

module_not_loaded::module_not_loaded(const char *name) noexcept
{
	moduleName = name;
}

const char *module_not_loaded::what() const noexcept
{
	std::string s;
	std::stringstream ss;

	ss << "Module " << moduleName << " not loaded!\n";
	ss >> s;

	return s.c_str();
}

bad_pointer::bad_pointer(const char *type) noexcept
{
	pointerType = type;
}

const char *bad_pointer::what() const noexcept
{
	std::string s;
	std::stringstream ss;

	ss << "Invalid pointer to " << pointerType << "!\n";
	ss >> s;

	return s.c_str();
}