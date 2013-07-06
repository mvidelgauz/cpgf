#ifndef __CPPINVOKABLE_H
#define __CPPINVOKABLE_H

#include "cppitem.h"
#include "cpptype.h"

#include <string>


class CppInvokable : public CppNamedItem
{
private:
	typedef CppNamedItem super;

public:
	explicit CppInvokable(const clang::Decl * decl);
	
	bool isStatic() const;
	bool isConst() const;
	bool isVariadic() const;
	bool isTemplate() const;
	bool isOverloaded() const;
	bool hasResult() const;

	size_t getArity() const;
	CppType getParamType(size_t index) const;
	bool paramHasDefaultValue(size_t index) const;
	std::string getTextOfParamDeafultValue(size_t index) const;

	std::string getTextOfPointeredType() const;
	std::string getTextOfParamList(const ItemTextOptionFlags & options) const;

	CppType getFunctionType() const;
	CppType getResultType() const;
};


#endif
