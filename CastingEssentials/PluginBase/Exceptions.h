#pragma once

#include <sstream>
#include <string>

class invalid_class_prop : public std::exception
{
public:
	invalid_class_prop(const char *name) noexcept;
	virtual const char *what() const noexcept;
private:
	const char *className;
};

inline invalid_class_prop::invalid_class_prop(const char *name) noexcept
{
	className = name;
}

inline const char *invalid_class_prop::what() const noexcept
{
	std::string s;
	std::stringstream ss;

	ss << "Attempted to access invalid property in entity class " << className << "!\n";
	ss >> s;

	return s.c_str();
}

class mismatching_entity_offset final : public std::runtime_error
{
public:
	mismatching_entity_offset(const char* expected, const char* actual) noexcept :
		std::runtime_error(BuildMessage(expected, actual).c_str())
	{
	}

private:
	static std::string BuildMessage(const char* expected, const char* actual)
	{
		std::stringstream ss;
		ss << "Attempted to use an EntityOffset intended for " << expected << " on " << actual;
		return ss.str();
	}
};

class module_not_loaded : public std::exception
{
public:
	module_not_loaded(const char *name) noexcept;
	virtual const char *what() const noexcept;
private:
	const char *moduleName;
};

inline module_not_loaded::module_not_loaded(const char *name) noexcept
{
	moduleName = name;
}

inline const char *module_not_loaded::what() const noexcept
{
	std::string s;
	std::stringstream ss;

	ss << "Module " << moduleName << " not loaded!\n";
	ss >> s;

	return s.c_str();
}

class bad_pointer : public std::exception
{
public:
	bad_pointer(const char *type) noexcept;
	virtual const char *what() const noexcept;
private:
	const char *pointerType;
};

inline bad_pointer::bad_pointer(const char *type) noexcept
{
	pointerType = type;
}

inline const char *bad_pointer::what() const noexcept
{
	std::string s;
	std::stringstream ss;

	ss << "Invalid pointer to " << pointerType << "!\n";
	ss >> s;

	return s.c_str();
}

class not_supported : public std::exception
{
public:
	not_supported(const char* funcname) noexcept { m_FuncName = funcname; }
	virtual const char* what() const noexcept override { return m_FuncName; }

private:
	const char* m_FuncName;
};