#pragma once

#include <string>

class invalid_class_prop : public std::exception
{
public:
	invalid_class_prop(const char *name) noexcept;
	virtual const char *what() const noexcept;
private:
	const char *className;
};

class mismatching_entity_offset final : public std::runtime_error
{
public:
	mismatching_entity_offset(const char* expected, const char* actual) noexcept;

private:
	static std::string BuildMessage(const char* expected, const char* actual);
};

class module_not_loaded : public std::exception
{
public:
	module_not_loaded(const char *name) noexcept;
	virtual const char *what() const noexcept;
private:
	const char *moduleName;
};

class bad_pointer : public std::exception
{
public:
	bad_pointer(const char *type) noexcept;
	virtual const char *what() const noexcept;
private:
	const char *pointerType;
};

class not_supported : public std::exception
{
public:
	not_supported(const char* funcname) noexcept { m_FuncName = funcname; }
	virtual const char* what() const noexcept override { return m_FuncName; }

private:
	const char* m_FuncName;
};