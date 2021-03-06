// Copyright (C) SAS NET Azure Rangers
// All rights reserved.

#pragma once
#include "Element.h"

class AFX_EXT_CLASS CElementFactory
{
public:
	CElementFactory(void);
	virtual ~CElementFactory(void);
};

class CFactory
{
public:
	static std::shared_ptr<CElement> CreateElementOfType(ElementType type, ShapeType shapeType);
	// counter of objects
	static int g_counter;
};


class AFX_EXT_CLASS CGuid
{
public:
	CGuid();
	virtual ~CGuid(void);

public:
	CString ToString();

private:
	UUID m_uuid;

};

