#include "cpgf/scriptbind/gscriptbind.h"
#include "cpgf/gstringmap.h"
#include "cpgf/gerrorcode.h"

#include "pinclude/gbindcommon.h"
#include "pinclude/gscriptbindapiimpl.h"

#include <string>

// for test
#include "cpgf/gmetadefine.h"
#include "cpgf/gmetaapi.h"
#include "cpgf/gmetaapiutil.h"
#include "cpgf/gmetaclass.h"
#include "cpgf/gscopedinterface.h"
#include "iostream"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4127 4100 4800 4512 4480 4267)
#endif

#include "jsapi.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

using namespace std;
using namespace cpgf::bind_internal;
using namespace JS;

namespace cpgf {

namespace {

//*********************************************
// Declarations
//*********************************************

#define ENTER_SPIDERMONKEY() \
	char local_msg[256]; bool local_error = false; { \
	try {

#define LEAVE_SPIDERMONKEY(...) \
	} \
	catch(const GException & e) { strncpy(local_msg, e.getMessage(), 256); local_error = true; } \
	catch(const exception & e) { strncpy(local_msg, e.what(), 256); local_error = true; } \
	catch(...) { strcpy(local_msg, "Unknown exception occurred."); local_error = true; } \
	} if(local_error) { local_msg[255] = 0; error(jsContext, local_msg); } \
	__VA_ARGS__;

typedef JS::Value JsValue;

class GSpiderBindingContext : public GBindingContext, public GShareFromBase
{
private:
	typedef GBindingContext super;

public:
	GSpiderBindingContext(IMetaService * service, const GScriptConfig & config, JSContext * jsContext, JSObject  * jsObject)
		: super(service, config), jsContext(jsContext), jsObject(jsObject)
	{
		if(this->jsObject == NULL) {
			this->jsObject = JS_NewObject(jsContext, NULL, NULL, NULL);
		}
	}

	JSContext * getJsContext() const {
		return this->jsContext;
	}

	JSObject  * getJsGlobalObject() const {
		return this->jsObject;
	}

	GGlueDataWrapperPool * getGlueDataWrapperPool() const {
		return &this->glueDataWrapperPool;
	}

private:
	JSContext * jsContext;
	JSObject  * jsObject;
	mutable GGlueDataWrapperPool glueDataWrapperPool;
};

typedef GSharedPointer<GSpiderBindingContext> GSpiderContextPointer;

class GSpiderScriptFunction : public GScriptFunctionBase
{
private:
	typedef GScriptFunctionBase super;

public:
	GSpiderScriptFunction(const GSpiderContextPointer & context, JSObject * self, const char * name);
	virtual ~GSpiderScriptFunction();

	virtual GVariant invoke(const GVariant * params, size_t paramCount);
	virtual GVariant invokeIndirectly(GVariant const * const * params, size_t paramCount);

private:
	JSObject * self;
	string name;
};

class GSpiderMonkeyScriptObject : public GScriptObjectBase
{
private:
	typedef GScriptObjectBase super;

public:
	GSpiderMonkeyScriptObject(IMetaService * service, const GScriptConfig & config, JSContext *jsContext, JSObject  * jsObject);
	GSpiderMonkeyScriptObject(const GSpiderMonkeyScriptObject & other, JSContext *jsContext, JSObject  * jsObject);
	virtual ~GSpiderMonkeyScriptObject();

	virtual void bindClass(const char * name, IMetaClass * metaClass);
	virtual void bindEnum(const char * name, IMetaEnum * metaEnum);

	virtual void bindFundamental(const char * name, const GVariant & value);
	virtual void bindString(const char * stringName, const char * s);
	virtual void bindObject(const char * objectName, void * instance, IMetaClass * type, bool transferOwnership);
	virtual void bindRaw(const char * name, const GVariant & value);
	virtual void bindMethod(const char * name, void * instance, IMetaMethod * method);
	virtual void bindMethodList(const char * name, IMetaList * methodList);

	virtual IMetaClass * getClass(const char * className);
	virtual IMetaEnum * getEnum(const char * enumName);

	virtual GVariant getFundamental(const char * name);
	virtual std::string getString(const char * stringName);
	virtual void * getObject(const char * objectName);
	virtual GVariant getRaw(const char * name);
	virtual IMetaMethod * getMethod(const char * methodName, void ** outInstance);
	virtual IMetaList * getMethodList(const char * methodName);

	virtual GScriptDataType getType(const char * name, IMetaTypedItem ** outMetaTypeItem);

	virtual GScriptObject * doCreateScriptObject(const char * name);

	virtual GScriptFunction * gainScriptFunction(const char * name);

	virtual GVariant invoke(const char * name, const GVariant * params, size_t paramCount);
	virtual GVariant invokeIndirectly(const char * name, GVariant const * const * params, size_t paramCount);

	virtual void assignValue(const char * fromName, const char * toName);
	virtual bool valueIsNull(const char * name);
	virtual void nullifyValue(const char * name);

	virtual void bindAccessible(const char * name, void * instance, IMetaAccessible * accessible);

	virtual void bindCoreService(const char * name, IScriptLibraryLoader * libraryLoader);

	GSpiderContextPointer getSpiderContext() const {
		return sharedStaticCast<GSpiderBindingContext>(this->getContext());
	}
	
private:	
	GMethodGlueDataPointer doGetMethodData(const char * methodName);
	
private:
	JSContext *jsContext;
	JSObject  * jsObject;
};

class JsClassUserData : public GUserData
{
private:
	typedef GUserData super;

public:
	explicit JsClassUserData(const char * className);
	
	JSClass * getJsClass() {
		return &this->jsClass;
	}

private:
	GScopedArray<char> className;
	JSClass jsClass;
};

JSObject * doBindClass(const GSpiderContextPointer & context, JSObject * owner, const char * name, IMetaClass * metaClass);
void doBindMethodList(const GSpiderContextPointer & context, const GClassGlueDataPointer & classData, JSObject * owner, const char * name, IMetaList * methodList, GGlueDataMethodType methodType);
JSFunction * createJsFunction(const GSpiderContextPointer & context, const GClassGlueDataPointer & classData, const char * name, IMetaList * methodList, GGlueDataMethodType methodType);
JsValue objectToSpider(const GSpiderContextPointer & context, const GClassGlueDataPointer & classData,
				 const GVariant & instance, const GBindValueFlags & flags, ObjectPointerCV cv, GGlueDataPointer * outputGlueData);
JsValue variantToSpider(const GContextPointer & context, const GVariant & data, const GBindValueFlags & flags, GGlueDataPointer * outputGlueData);
JsValue rawToSpider(const GSpiderContextPointer & context, const GVariant & value, GGlueDataPointer * outputGlueData);
JSObject * createEnumBinding(const GSpiderContextPointer & context, IMetaEnum * metaEnum);
JSObject * doBindEnum(const GSpiderContextPointer & context, JSObject * owner, const char * name, IMetaEnum * metaEnum);


//*********************************************
// Global function implementations
//*********************************************


void error(JSContext * jsContext, const char * message)
{
	JS_ReportError(jsContext, "%s", message);
}

typedef map<void *, void *> ObjectPrivateDataMap;
void setPrivateData(ObjectPrivateDataMap * dataMap, void * object, void * data)
{
	(*dataMap)[object] = data;
}

void * getPrivateData(ObjectPrivateDataMap * dataMap, void * object)
{
	ObjectPrivateDataMap::iterator it = dataMap->find(object);
	if(it == dataMap->end()) {
		return NULL;
	}
	else {
		return it->second;
	}
}

ObjectPrivateDataMap objectPrivateDataMap;
ObjectPrivateDataMap functionPrivateDataMap;

void setClassPrivateData(JSContext * jsContext, JSObject * object, void * data)
{
	JSObject * proto = JS_GetObjectPrototype(jsContext, object);
	if(proto != NULL) {
		JS_SetPrivate(proto, data);
	}
	else {
		GASSERT(false);
	}
}

void * getClassPrivateData(JSContext * jsContext, JSObject * object)
{
	JSObject * proto = JS_GetObjectPrototype(jsContext, object);
	if(proto == NULL) {
		return NULL;
	}
	return JS_GetPrivate(proto);
}

void setObjectPrivateData(JSContext * jsContext, JSObject * object, void * data)
{
	JS_SetPrivate(object, data);
}

void * getObjectPrivateData(JSContext * jsContext, JSObject * object)
{
	return JS_GetPrivate(object);
}

void * getObjectOrClassPrivateData(JSContext * jsContext, JSObject * object)
{
	void * data = getObjectPrivateData(jsContext, object);
	if(data == NULL) {
		data = getClassPrivateData(jsContext, object);
	}
	return data;
}

void setFunctionPrivateData(JSContext * jsContext, JSObject * object, void * data)
{
	setPrivateData(&functionPrivateDataMap, object, data);
}

void * getFunctionPrivateData(JSContext * jsContext, JSObject * object)
{
	return getPrivateData(&functionPrivateDataMap, object);
}

static const JsValue dummy;
struct GSpiderMethods
{
	typedef JsValue ResultType;

	static ResultType doObjectToScript(const GContextPointer & context, const GClassGlueDataPointer & classData,
		const GVariant & instance, const GBindValueFlags & flags, ObjectPointerCV cv, GGlueDataPointer * outputGlueData)
	{
		return objectToSpider(sharedStaticCast<GSpiderBindingContext>(context), classData, instance, flags, cv, outputGlueData);
	}

	static ResultType doVariantToScript(const GContextPointer & context, const GVariant & value, const GBindValueFlags & flags, GGlueDataPointer * outputGlueData)
	{
		return variantToSpider(context, value, flags, outputGlueData);
	}
	
	static ResultType doRawToScript(const GContextPointer & context, const GVariant & value, GGlueDataPointer * outputGlueData)
	{
		return rawToSpider(sharedStaticCast<GSpiderBindingContext>(context), value, outputGlueData);
	}

	static ResultType doClassToScript(const GContextPointer & context, IMetaClass * metaClass)
	{
		GSpiderContextPointer spiderContext = sharedStaticCast<GSpiderBindingContext>(context);
		JSObject * classObject = doBindClass(spiderContext, spiderContext->getJsGlobalObject(), metaClass->getName(), metaClass);
		return ObjectValue(*classObject);
	}

	static ResultType doStringToScript(const GContextPointer & context, const char * s)
	{
		JSString * jsString = JS_NewStringCopyZ(sharedStaticCast<GSpiderBindingContext>(context)->getJsContext(), s);
		return StringValue(jsString);
	}

	static ResultType doWideStringToScript(const GContextPointer & context, const wchar_t * ws)
	{
		JSString * jsString = JS_NewUCStringCopyZ(sharedStaticCast<GSpiderBindingContext>(context)->getJsContext(), ws);
		return StringValue(jsString);
	}

	static bool isSuccessResult(const ResultType & result)
	{
		return ! result.isUndefined();
	}

	static ResultType doMethodsToScript(const GClassGlueDataPointer & classData, GMetaMapItem * mapItem,
		const char * methodName, GMetaClassTraveller * /*traveller*/,
		IMetaClass * metaClass, IMetaClass * derived, const GObjectGlueDataPointer & objectData)
	{
		GMapItemMethodData * data = gdynamic_cast<GMapItemMethodData *>(mapItem->getUserData());
		GSpiderContextPointer context = sharedStaticCast<GSpiderBindingContext>(classData->getContext());
		if(data == NULL) {
			GScopedInterface<IMetaClass> boundClass(selectBoundClass(metaClass, derived));
			GMethodGlueDataPointer glueData = context->newMethodGlueData(context->getClassData(boundClass.get()), NULL, methodName, gdmtInternal);
			data = new GMapItemMethodData(glueData);
			mapItem->setUserData(data);
		}
		JSFunction * jsFunction = createJsFunction(context, classData, methodName, data->getMethodData()->getMethodList(), gdmtMethodList);
		JSObject * functionObject = JS_GetFunctionObject(jsFunction);
		return ObjectValue(*functionObject);
	}
	
	static ResultType doEnumToScript(const GClassGlueDataPointer & classData, GMetaMapItem * mapItem, const char * /*enumName*/)
	{
		GScopedInterface<IMetaEnum> metaEnum(gdynamic_cast<IMetaEnum *>(mapItem->getItem()));
		JSObject * enumObject = createEnumBinding(sharedStaticCast<GSpiderBindingContext>(classData->getContext()), metaEnum.get());
		return ObjectValue(*enumObject );
	}

};

JsValue variantToSpider(const GContextPointer & context, const GVariant & data, const GBindValueFlags & flags, GGlueDataPointer * outputGlueData)
{
	GVariant value = getVariantRealValue(data);
	GMetaType type = getVariantRealMetaType(data);

	GVariantType vt = static_cast<GVariantType>(value.getType() & ~byReference);

	if(vtIsEmpty(vt)) {
		return JSVAL_NULL;
	}

	if(vtIsBoolean(vt)) {
		return BooleanValue(fromVariant<bool>(value));
	}

	if(vtIsInteger(vt)) {
		return Int32Value(fromVariant<int>(value));
	}

	if(vtIsReal(vt)) {
		return DoubleValue(fromVariant<double>(value));
	}

	if(!vtIsInterface(vt) && canFromVariant<void *>(value) && objectAddressFromVariant(value) == NULL) {
		return JSVAL_NULL;
	}

	if(variantIsString(value)) {
		char * s = fromVariant<char *>(value);
		JSString * jsString = JS_NewStringCopyZ(sharedStaticCast<GSpiderBindingContext>(context)->getJsContext(), s);
		return StringValue(jsString);
	}

	if(variantIsWideString(value)) {
		const wchar_t * ws = fromVariant<wchar_t *>(value);
		JSString * jsString = JS_NewUCStringCopyZ(sharedStaticCast<GSpiderBindingContext>(context)->getJsContext(), ws);
		return StringValue(jsString);
	}

	return complexVariantToScript<GSpiderMethods>(context, value, type, flags, outputGlueData);
}

GVariant spiderUserDataToVariant(const GSpiderContextPointer & context, JsValue value, GGlueDataPointer * outputGlueData)
{
	if(value.isObject()) {
		JSObject * object = &value.toObject();
		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getObjectPrivateData(context->getJsContext(), object));
		if(dataWrapper != NULL) {
			GGlueDataPointer glueData = dataWrapper->getData();
			if(outputGlueData != NULL) {
				*outputGlueData = glueData;
			}
			return glueDataToVariant(glueData);
		}
	}

	return GVariant();
}

GVariant spiderToVariant(const GSpiderContextPointer & context, JsValue value, GGlueDataPointer * outputGlueData)
{
	if(value.isUndefined()) {
		return GVariant();
	}

	if(value.isBoolean()) {
		return value.toBoolean();
	}

	if(value.isInt32()) {
		return value.toInt32();
	}

	if(value.isNull()) {
		return (void *)0;
	}

	if(value.isNumber()) {
		return value.toNumber();
	}

	if(value.isString()) {
		JSString * jsString = value.toString();
		GScopedArray<char> s(wideStringToString(JS_GetStringCharsZ(context->getJsContext(), jsString)));
		return createTypedVariant(createStringVariant(s.get()), createMetaType<char *>());
	}

	if(value.isObject()) {
		return spiderUserDataToVariant(context, value, outputGlueData);
	}

	return GVariant();
}

JsValue rawToSpider(const GSpiderContextPointer & context, const GVariant & value, GGlueDataPointer * outputGlueData)
{
	GRawGlueDataPointer rawData(context->newRawGlueData(value));
	GGlueDataWrapper * dataWrapper = newGlueDataWrapper(rawData, context->getGlueDataWrapperPool());

	JSObject * object = JS_NewObject(context->getJsContext(), NULL, NULL, NULL);
	setObjectPrivateData(context->getJsContext(), object, dataWrapper);

	if(outputGlueData != NULL) {
		*outputGlueData = rawData;
	}

	return ObjectValue(*object);
}

void * spiderToObject(JSContext * jsContext, JsValue value)
{
	if(value.isObject()) {
		JSObject * object = &value.toObject();
		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getObjectPrivateData(jsContext, object));
		if(dataWrapper != NULL && dataWrapper->getData()->getType() == sdtObject) {
			return dataWrapper->getAs<GObjectGlueData>()->getInstanceAddress();
		}
	}

	return NULL;
}

GScriptDataType getSpiderType(const GSpiderContextPointer & context, JsValue value, IMetaTypedItem ** typeItem)
{
	if(typeItem != NULL) {
		*typeItem = NULL;
	}

	if(value.isNull()) {
		return sdtNull;
	}

	if(value.isUndefined()) {
		return sdtNull;
	}

	if(value.isBoolean()) {
		return sdtFundamental;
	}

	if(value.isInt32() || value.isNumber()) {
		return sdtFundamental;
	}

	if(value.isString()) {
		return sdtString;
	}

	if(value.isObject()) {
		JSObject * jsObject = &value.toObject();
		JSContext * jsContext = context->getJsContext();

		GGlueDataWrapper * dataWrapper;

		dataWrapper = static_cast<GGlueDataWrapper *>(getClassPrivateData(jsContext, jsObject));
		if(dataWrapper != NULL) {
			return sdtClass;
		}
		dataWrapper = static_cast<GGlueDataWrapper *>(getObjectPrivateData(jsContext, jsObject));
		if(dataWrapper != NULL) {
			switch(dataWrapper->getData()->getType()) {
				case gdtObject: {
					if(typeItem != NULL) {
						*typeItem = dataWrapper->getAs<GObjectGlueData>()->getClassData()->getMetaClass();
						(*typeItem)->addReference();
					}
					return sdtObject;
				}

				break;

				case gdtEnum:
					if(typeItem != NULL) {
						*typeItem = dataWrapper->getAs<GEnumGlueData>()->getMetaEnum();
						(*typeItem)->addReference();
					}
					return sdtEnum;

			    case gdtRaw:
			    	return sdtRaw;

				default:
					break;
			}
		}
		dataWrapper = static_cast<GGlueDataWrapper *>(getFunctionPrivateData(jsContext, jsObject));
		if(dataWrapper != NULL) {
			return methodTypeToGlueDataType(dataWrapper->getAs<GMethodGlueData>()->getMethodType());
		}

		if(JS_ObjectIsFunction(jsContext, jsObject)) {
			return sdtScriptMethod;
		}
		else {
			return sdtScriptObject;
		}
	}

	return sdtUnknown;
}

void loadCallableParam(jsval * valuePointer, const GSpiderContextPointer & context, InvokeCallableParam * callableParam)
{
	for(int i = 0; i < callableParam->paramCount; ++i) {
		JsValue value = JS_ARGV(context->getJsContext(), valuePointer)[i];
		callableParam->params[i].value = spiderToVariant(context, value , &callableParam->params[i].glueData);
		IMetaTypedItem * typeItem;
		callableParam->params[i].dataType = getSpiderType(context, value , &typeItem);
		callableParam->params[i].typeItem.reset(typeItem);
	}
}

JSBool callbackMethodList(JSContext * jsContext, unsigned int argc, jsval * valuePointer)
{
	ENTER_SPIDERMONKEY()

	JSObject * selfObject = JS_THIS_OBJECT(jsContext, valuePointer);
	JsValue callee = JS_CALLEE(jsContext, valuePointer);
	if(callee.isObject()) {
		JSObject * functionObject = &callee.toObject();
		GGlueDataWrapper * methodDataWrapper = static_cast<GGlueDataWrapper *>(getFunctionPrivateData(jsContext, functionObject));
		GMethodGlueDataPointer methodData(methodDataWrapper->getAs<GMethodGlueData>());
		
		GObjectGlueDataPointer objectData;
		GGlueDataWrapper * objectDataWrapper = static_cast<GGlueDataWrapper *>(getObjectOrClassPrivateData(jsContext, selfObject));
		if(objectDataWrapper->getData()->getType() == gdtObject) {
			objectData = objectDataWrapper->getAs<GObjectGlueData>();
		}
		
		InvokeCallableParam callableParam(argc);
		loadCallableParam(valuePointer, sharedStaticCast<GSpiderBindingContext>(methodData->getContext()), &callableParam);

		InvokeCallableResult result = doInvokeMethodList(methodData->getContext(), objectData, methodData, &callableParam);
		JsValue resultValue;
		if(result.resultCount > 0) {
			resultValue = methodResultToScript<GSpiderMethods>(methodData->getContext(), result.callable.get(), &result);
		}
		else {
			resultValue = JSVAL_VOID;
		}
		JS_SET_RVAL(jsContext, valuePointer, resultValue);
		return JS_TRUE;
	}
	
	return JS_FALSE;

	LEAVE_SPIDERMONKEY(return JS_FALSE)
}

void doBindMethodList(const GSpiderContextPointer & context, const GClassGlueDataPointer & classData, JSObject * owner, const char * name, IMetaList * methodList, GGlueDataMethodType methodType)
{
	JSFunction * jsFunction = createJsFunction(context, classData, name, methodList, methodType);
	JSObject * functionObject = JS_GetFunctionObject(jsFunction);
	JsValue value = ObjectValue(*functionObject);
	JS_DefineProperty(context->getJsContext(), owner, name, value, NULL, NULL, 0);
}

JSFunction * createJsFunction(const GSpiderContextPointer & context, const GClassGlueDataPointer & classData, const char * name, IMetaList * methodList, GGlueDataMethodType methodType)
{
	GMethodGlueDataPointer glueData = context->newMethodGlueData(classData, methodList, name, methodType);
	GGlueDataWrapper * dataWrapper = newGlueDataWrapper(glueData, context->getGlueDataWrapperPool());

	JSContext * jsContext = context->getJsContext();
	JSFunction * jsFunction = JS_NewFunction(jsContext, &callbackMethodList, 0, 0, NULL, name);

	JSObject * functionObject = JS_GetFunctionObject(jsFunction);
	setFunctionPrivateData(context->getJsContext(), functionObject, dataWrapper);

	return jsFunction;
}

JsValue objectToSpider(const GSpiderContextPointer & context, const GClassGlueDataPointer & classData,
				 const GVariant & instance, const GBindValueFlags & flags, ObjectPointerCV cv, GGlueDataPointer * outputGlueData)
{
	if(objectAddressFromVariant(instance) == NULL) {
		return JSVAL_NULL;
	}

	GObjectGlueDataPointer objectData = context->newObjectGlueData(classData, instance, flags, cv);
	GGlueDataWrapper * objectWrapper = newGlueDataWrapper(objectData, sharedStaticCast<GSpiderBindingContext>(context)->getGlueDataWrapperPool());

	if(outputGlueData != NULL) {
		*outputGlueData = objectData;
	}

	IMetaClass * metaClass = classData->getMetaClass();
	GMetaMapClass * mapClass = classData->getClassMap();
	JsClassUserData * classUserData = static_cast<JsClassUserData *>(mapClass->getUserData());
	JSObject * object = JS_NewObject(context->getJsContext(), classUserData->getJsClass(), NULL, NULL);
	setObjectPrivateData(context->getJsContext(), object, objectWrapper);

	return ObjectValue(*object);
}

JSBool propertyGetter(JSContext *jsContext, JSHandleObject obj, JSHandleId id, JSMutableHandleValue vp)
{
	ENTER_SPIDERMONKEY()

	JsValue idValue;
	if(! JS_IdToValue(jsContext, id, &idValue)) {
		return JS_FALSE;
	}
	if(idValue.isString()) {
		JSString * jsString = idValue.toString();
		GScopedArray<char> name(wideStringToString(JS_GetStringCharsZ(jsContext, jsString)));

		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getObjectOrClassPrivateData(jsContext, obj));
	
		vp.set(namedMemberToScript<GSpiderMethods>(dataWrapper->getData(), name.get()));
		return JS_TRUE;
	}

	return JS_FALSE;

	LEAVE_SPIDERMONKEY(return JS_FALSE)
}

JSBool propertySetter(JSContext * jsContext, JSHandleObject obj, JSHandleId id, JSBool strict, JSMutableHandleValue vp)
{
	ENTER_SPIDERMONKEY()

	JsValue idValue;
	if(! JS_IdToValue(jsContext, id, &idValue)) {
		return JS_FALSE;
	}
	if(idValue.isString()) {
		JSString * jsString = idValue.toString();
		GScopedArray<char> name(wideStringToString(JS_GetStringCharsZ(jsContext, jsString)));

		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getObjectOrClassPrivateData(jsContext, obj));
	
		if(getGlueDataCV(dataWrapper->getData()) == opcvConst) {
			raiseCoreException(Error_ScriptBinding_CantWriteToConstObject);
		}
		else {
			GVariant v;
			GGlueDataPointer valueGlueData;

			v = spiderToVariant(sharedStaticCast<GSpiderBindingContext>(dataWrapper->getData()->getContext()), vp, &valueGlueData);
			if(setValueOnNamedMember(dataWrapper->getData(), name.get(), v, valueGlueData)) {
				return JS_TRUE;
			}
		}
	}

	return JS_FALSE;

	LEAVE_SPIDERMONKEY(return JS_FALSE)
}

JSBool enumGetter(JSContext *jsContext, JSHandleObject obj, JSHandleId id, JSMutableHandleValue vp)
{
	ENTER_SPIDERMONKEY()

	JsValue idValue;
	if(! JS_IdToValue(jsContext, id, &idValue)) {
		return JS_FALSE;
	}
	if(idValue.isString()) {
		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getObjectOrClassPrivateData(jsContext, obj));
		if(dataWrapper->getData()->getType() == sdtEnum) {
			JSString * jsString = idValue.toString();
			GScopedArray<char> name(wideStringToString(JS_GetStringCharsZ(jsContext, jsString)));

			IMetaEnum * metaEnum = dataWrapper->getAs<GEnumGlueData>()->getMetaEnum();
			int32_t index = metaEnum->findKey(name.get());
			if(index >= 0) {
				vp.set(variantToSpider(dataWrapper->getData()->getContext(), metaGetEnumValue(metaEnum, index), GBindValueFlags(), NULL));
			}
			else {
				raiseCoreException(Error_ScriptBinding_CantFindEnumKey, *name);
			}
			return JS_TRUE;
		}
	}

	return JS_FALSE;

	LEAVE_SPIDERMONKEY(return JS_FALSE)
}

JSBool enumSetter(JSContext * jsContext, JSHandleObject obj, JSHandleId id, JSBool strict, JSMutableHandleValue vp)
{
	ENTER_SPIDERMONKEY()

	raiseCoreException(Error_ScriptBinding_CantAssignToEnumMethodClass);

	return JS_FALSE;

	LEAVE_SPIDERMONKEY(return JS_FALSE)
}

JSBool accessibleGetter(JSContext *jsContext, JSHandleObject obj, JSHandleId id, JSMutableHandleValue vp)
{
	ENTER_SPIDERMONKEY()

	JsValue idValue;
	if(! JS_IdToValue(jsContext, id, &idValue)) {
		return JS_FALSE;
	}
	if(idValue.isString()) {
		JSString * jsString = idValue.toString();
		GScopedArray<char> name(wideStringToString(JS_GetStringCharsZ(jsContext, jsString)));

		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getObjectPrivateData(jsContext, obj));
		if(dataWrapper->getData()->getType() == gdtAccessible) {
			GAccessibleGlueDataPointer accessibleGlueData = dataWrapper->getAs<GAccessibleGlueData>();
			vp.set(accessibleToScript<GSpiderMethods>(accessibleGlueData->getContext(), accessibleGlueData->getAccessible(), accessibleGlueData->getInstanceAddress(), false));
	
			return JS_TRUE;
		}
	}

	return JS_FALSE;

	LEAVE_SPIDERMONKEY(return JS_FALSE)
}

JSBool accessibleSetter(JSContext * jsContext, JSHandleObject obj, JSHandleId id, JSBool strict, JSMutableHandleValue vp)
{
	ENTER_SPIDERMONKEY()

	JsValue idValue;
	if(! JS_IdToValue(jsContext, id, &idValue)) {
		return JS_FALSE;
	}
	if(idValue.isString()) {
		JSString * jsString = idValue.toString();
		GScopedArray<char> name(wideStringToString(JS_GetStringCharsZ(jsContext, jsString)));

		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getObjectPrivateData(jsContext, obj));
		if(dataWrapper->getData()->getType() == gdtAccessible) {
			GAccessibleGlueDataPointer accessibleGlueData = dataWrapper->getAs<GAccessibleGlueData>();
			GVariant v = spiderToVariant(sharedStaticCast<GSpiderBindingContext>(accessibleGlueData->getContext()), vp, NULL);
			metaSetValue(accessibleGlueData->getAccessible(), accessibleGlueData->getInstanceAddress(), v);
	
			return JS_TRUE;
		}
	}

	return JS_FALSE;

	LEAVE_SPIDERMONKEY(return JS_FALSE)
}

JSObject * createEnumBinding(const GSpiderContextPointer & context, IMetaEnum * metaEnum)
{
	GEnumGlueDataPointer enumGlueData(context->newEnumGlueData(metaEnum));
	GGlueDataWrapper * dataWrapper = newGlueDataWrapper(enumGlueData, context->getGlueDataWrapperPool());
	JSObject * enumObject = JS_NewObject(context->getJsContext(), NULL, NULL, NULL);
	setObjectPrivateData(context->getJsContext(), enumObject, dataWrapper);
	return enumObject;
}

JSObject * doBindEnum(const GSpiderContextPointer & context, JSObject * owner, const char * name, IMetaEnum * metaEnum)
{
	JSObject * enumObject = createEnumBinding(context, metaEnum);
	JS_DefineProperty(context->getJsContext(), owner, name, JSVAL_NULL, &enumGetter, &enumSetter, JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY);
	return enumObject;
}

void doBindProperty(const GSpiderContextPointer & context, JSObject * owner, const char * name, IMetaAccessible * accessible)
{
	unsigned int flags = JSPROP_PERMANENT | JSPROP_SHARED;
	if(! accessible->canSet()) {
		flags |= JSPROP_READONLY;
	}
	JS_DefineProperty(context->getJsContext(), owner, name, JSVAL_NULL, &propertyGetter, &propertySetter, flags);
}

void doBindAccessible(const GSpiderContextPointer & context, JSObject * owner, const char * name, void * instance, IMetaAccessible * accessible)
{
	unsigned int flags = JSPROP_PERMANENT | JSPROP_SHARED;
	if(! accessible->canSet()) {
		flags |= JSPROP_READONLY;
	}
	JS_DefineProperty(context->getJsContext(), owner, name, JSVAL_NULL, &accessibleGetter, &accessibleSetter, flags);
}

void doBindAllProperties(const GSpiderContextPointer & context, JSObject * nonStaticObject, JSObject * staticObject, const GClassGlueDataPointer & classData)
{
	const GMetaMapClass::MapType * mapData = classData->getClassMap()->getMap();
	GScopedInterface<IObject> metaObject;
	for(GMetaMapClass::MapType::const_iterator it = mapData->begin(); it != mapData->end(); ++it) {
		const char * name = it->first;
		const GMetaMapItem * item = &it->second;
		metaObject.reset(item->getItem());

		switch(item->getType()) {
			case mmitMethod: {
				IMetaMethod * method = gdynamic_cast<IMetaMethod *>(metaObject.get());
				GScopedInterface<IMetaList> metaList(createMetaList());
				metaList->add(method, NULL);
				bool isStatic = method->isStatic();
				JSObject * object = isStatic ? staticObject : nonStaticObject;
				doBindMethodList(context, classData, object, method->getName(), metaList.get(), gdmtMethod);
			}
				break;

			case mmitMethodList: {
				IMetaList * metaList = gdynamic_cast<IMetaList *>(metaObject.get());
				doBindMethodList(context, classData, nonStaticObject, name, metaList, gdmtMethodList);
				doBindMethodList(context, classData, staticObject, name, metaList, gdmtMethodList);
			}
				break;

			case mmitProperty:
			case mmitField: {
				IMetaAccessible * accessible = gdynamic_cast<IMetaAccessible *>(metaObject.get());
				bool isStatic = accessible->isStatic();
				JSObject * object = isStatic ? staticObject : nonStaticObject;
				doBindProperty(context, object, accessible->getName(), accessible);
			}
				break;

			case mmitEnum:
//				scriptObject->bindEnum(normalizeReflectName(name).c_str(), gdynamic_cast<IMetaEnum *>(metaObject.get()));
				break;

			case mmitEnumValue:
				break;

			case mmitClass: {
				IMetaClass * metaClass = gdynamic_cast<IMetaClass *>(metaObject.get());
				doBindClass(context, staticObject, name, metaClass);
			}
				break;

			default:
				break;
		}
	}
}

JSBool objectConstructor(JSContext * jsContext, unsigned int argc, jsval * valuePointer)
{
	ENTER_SPIDERMONKEY()

	JsValue callee = JS_CALLEE(jsContext, valuePointer);
	if(callee.isObject()) {
		JSObject * sObject = &callee.toObject();
		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getClassPrivateData(jsContext, sObject));
		if(dataWrapper == NULL) {
		}

		GClassGlueDataPointer classData = dataWrapper->getAs<GClassGlueData>();
		GSpiderContextPointer context = sharedStaticCast<GSpiderBindingContext>(classData->getContext());
		
		InvokeCallableParam callableParam(argc);
		loadCallableParam(valuePointer, context, &callableParam);

		void * instance = doInvokeConstructor(context, context->getService(), classData->getMetaClass(), &callableParam);

		if(instance != NULL) {
			JsValue object = objectToSpider(context, classData, instance, GBindValueFlags(bvfAllowGC), opcvNone, NULL);
			JS_SET_RVAL(jsContext, valuePointer, object);
		}
		else {
			raiseCoreException(Error_ScriptBinding_FailConstructObject);
		}

		return JS_TRUE;
	}

	return JS_FALSE;

	LEAVE_SPIDERMONKEY(return JS_FALSE)
}

JSObject * doBindClass(const GSpiderContextPointer & context, JSObject * owner, const char * name, IMetaClass * metaClass)
{
	GClassGlueDataPointer classData(context->newClassData(metaClass));
	GGlueDataWrapper * dataWrapper = newGlueDataWrapper(classData, context->getGlueDataWrapperPool());
	GMetaMapClass * mapClass = classData->getClassMap();
	JsClassUserData * classUserData = new JsClassUserData(name);
	mapClass->setUserData(classUserData);

	JSObject * classObject = JS_InitClass(context->getJsContext(), owner, owner,
		classUserData->getJsClass(), &objectConstructor, 0, NULL, NULL, NULL, NULL);

	JSObject * proto = JS_GetObjectPrototype(context->getJsContext(), classObject);
	doBindAllProperties(context, proto, classObject, classData);

	setClassPrivateData(context->getJsContext(), classObject, dataWrapper);

	return classObject;
}

JSBool jsResolve(JSContext * jsContext, JSObject *obj, jsval id) //, unsigned int flags, JSObject **objp)
{
	JSType type = JS_TypeOfValue(jsContext, id);
	if(id.isString()) {
		JSString * jsString = id.toString();
		GScopedArray<char> s(wideStringToString(JS_GetStringCharsZ(jsContext, jsString)));
	}
	float value = id.toNumber();
	return JS_TRUE;
}

//*********************************************
// Classes implementations
//*********************************************

static JSClass jsClassTemplate = {
    NULL,  /* name */
    0,  /* flags */
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JsClassUserData::JsClassUserData(const char * className) : super(), className(new char[strlen(className) + 1])
{
	strcpy(this->className.get(), className);

	this->jsClass = jsClassTemplate;

	this->jsClass.name = this->className.get();
	this->jsClass.flags = JSCLASS_HAS_PRIVATE; // | JSCLASS_NEW_RESOLVE | (1 << 5); // (1 << 5) is JSCLASS_NEW_RESOLVE_GETS_START;
	this->jsClass.addProperty = JS_PropertyStub;
	this->jsClass.delProperty = JS_PropertyStub;
	this->jsClass.getProperty = JS_PropertyStub;
	this->jsClass.setProperty = JS_StrictPropertyStub;
	this->jsClass.enumerate = JS_EnumerateStub;
	this->jsClass.resolve = (JSResolveOp)&jsResolve;
	this->jsClass.convert = JS_ConvertStub;
	this->jsClass.finalize = NULL;
	this->jsClass.checkAccess = NULL;
	this->jsClass.call = NULL;
	this->jsClass.construct = NULL;
	this->jsClass.hasInstance = NULL;
	this->jsClass.trace = NULL;
}


GSpiderScriptFunction::GSpiderScriptFunction(const GSpiderContextPointer & context, JSObject * self, const char * name)
	: super(context), self(self), name(name)
{
}

GSpiderScriptFunction::~GSpiderScriptFunction()
{
}

GVariant GSpiderScriptFunction::invoke(const GVariant * params, size_t paramCount)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");

	const GVariant * variantPointers[REF_MAX_ARITY];

	for(size_t i = 0; i < paramCount; ++i) {
		variantPointers[i] = &params[i];
	}

	return this->invokeIndirectly(variantPointers, paramCount);
}

GVariant invokeSpiderFunctionIndirectly(const GSpiderContextPointer & context, GVariant const * const * params, size_t paramCount, const char * name, JSObject * self)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");

	JSContext * jsContext = context->getJsContext();

	ENTER_SPIDERMONKEY()

	JsValue parameters[REF_MAX_ARITY];
	for(size_t i = 0; i < paramCount; ++i) {
		parameters[i] = variantToSpider(context, *params[i], GBindValueFlags(bvfAllowRaw), NULL);
		if(parameters[i].isUndefined()) {
			raiseCoreException(Error_ScriptBinding_ScriptMethodParamMismatch, i, name);
		}
	}
	JsValue result;
	if(JS_CallFunctionName(jsContext, self, name, paramCount, parameters, &result)) {
		return spiderToVariant(context, result, NULL);
	}

	LEAVE_SPIDERMONKEY(return GVariant())
}

GVariant GSpiderScriptFunction::invokeIndirectly(GVariant const * const * params, size_t paramCount)
{
	return invokeSpiderFunctionIndirectly(sharedStaticCast<GSpiderBindingContext>(this->getContext()), params, paramCount, this->name.c_str(), this->self);
}

	
GSpiderMonkeyScriptObject::GSpiderMonkeyScriptObject(IMetaService * service, const GScriptConfig & config, JSContext * jsContext, JSObject  * jsObject)
	: super(GContextPointer(new GSpiderBindingContext(service, config, jsContext, jsObject)), config), jsContext(jsContext)
{
	this->jsObject = sharedStaticCast<GSpiderBindingContext>(this->getContext())->getJsGlobalObject();
}

GSpiderMonkeyScriptObject::GSpiderMonkeyScriptObject(const GSpiderMonkeyScriptObject & other, JSContext *jsContext, JSObject  * jsObject)
	: super(GContextPointer(new GSpiderBindingContext(other.getContext()->getService(), other.getConfig(), jsContext, jsObject)), other.getConfig()), jsContext(jsContext)
{
	this->jsObject = sharedStaticCast<GSpiderBindingContext>(this->getContext())->getJsGlobalObject();
}

GSpiderMonkeyScriptObject::~GSpiderMonkeyScriptObject()
{
}

GScriptDataType GSpiderMonkeyScriptObject::getType(const char * name, IMetaTypedItem ** outMetaTypeItem)
{
	ENTER_SPIDERMONKEY()

	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, name, &value)) {
		return getSpiderType(this->getSpiderContext(), value, outMetaTypeItem);
	}
	return sdtUnknown;

	LEAVE_SPIDERMONKEY(return sdtUnknown)
}

void GSpiderMonkeyScriptObject::bindClass(const char * name, IMetaClass * metaClass)
{
	ENTER_SPIDERMONKEY()

	doBindClass(this->getSpiderContext(), this->jsObject, name, metaClass);

	LEAVE_SPIDERMONKEY()
}

void GSpiderMonkeyScriptObject::bindEnum(const char * name, IMetaEnum * metaEnum)
{
	ENTER_SPIDERMONKEY()

	doBindEnum(this->getSpiderContext(), this->jsObject, name, metaEnum);

	LEAVE_SPIDERMONKEY()
}

GScriptObject * GSpiderMonkeyScriptObject::doCreateScriptObject(const char * name)
{
	ENTER_SPIDERMONKEY()

	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, name, &value)) {
		if(value.isObject()) {
			JSObject * object = &value.toObject();
			GSpiderMonkeyScriptObject * newScriptObject = new GSpiderMonkeyScriptObject(*this, this->jsContext, object);
			newScriptObject->setOwner(this);
			newScriptObject->setName(name);
			return newScriptObject;
		}
	}
	else {
		JSObject * object = JS_NewObject(this->jsContext, NULL, NULL, NULL);
		GSpiderMonkeyScriptObject * newScriptObject = new GSpiderMonkeyScriptObject(*this, this->jsContext, object);
		newScriptObject->setOwner(this);
		newScriptObject->setName(name);
		return newScriptObject;
	}

	return NULL;

	LEAVE_SPIDERMONKEY(return NULL)
}

GScriptFunction * GSpiderMonkeyScriptObject::gainScriptFunction(const char * name)
{
	ENTER_SPIDERMONKEY()

	return NULL;

	LEAVE_SPIDERMONKEY(return NULL)
}

GVariant GSpiderMonkeyScriptObject::invoke(const char * name, const GVariant * params, size_t paramCount)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");

	const cpgf::GVariant * variantPointers[REF_MAX_ARITY];

	for(size_t i = 0; i < paramCount; ++i) {
		variantPointers[i] = &params[i];
	}

	return this->invokeIndirectly(name, variantPointers, paramCount);
}

GVariant GSpiderMonkeyScriptObject::invokeIndirectly(const char * name, GVariant const * const * params, size_t paramCount)
{
	ENTER_SPIDERMONKEY()

	return invokeSpiderFunctionIndirectly(this->getSpiderContext(), params, paramCount, name, this->jsObject);

	LEAVE_SPIDERMONKEY(return GVariant())
}

void GSpiderMonkeyScriptObject::bindFundamental(const char * name, const GVariant & value)
{
	GASSERT_MSG(vtIsFundamental(vtGetType(value.refData().typeData)), "Only fundamental value can be bound via bindFundamental");

	ENTER_SPIDERMONKEY()

	JS_DefineProperty(this->jsContext, this->jsObject, name, variantToSpider(this->getContext(), value, GBindValueFlags(bvfAllowRaw), NULL), NULL, NULL, 0);
	
	LEAVE_SPIDERMONKEY()
}

void GSpiderMonkeyScriptObject::bindAccessible(const char * name, void * instance, IMetaAccessible * accessible)
{
	ENTER_SPIDERMONKEY()

	doBindAccessible(this->getSpiderContext(), this->jsObject, name, instance, accessible);

	LEAVE_SPIDERMONKEY()
}

void GSpiderMonkeyScriptObject::bindString(const char * stringName, const char * s)
{
	ENTER_SPIDERMONKEY()

	JSString * jsString = JS_NewStringCopyZ(this->jsContext, s);
	JsValue value = StringValue(jsString);
	JS_DefineProperty(this->jsContext, this->jsObject, stringName, value, NULL, NULL, 0);

	LEAVE_SPIDERMONKEY()
}

void GSpiderMonkeyScriptObject::bindObject(const char * objectName, void * instance, IMetaClass * type, bool transferOwnership)
{
	ENTER_SPIDERMONKEY()

	LEAVE_SPIDERMONKEY()
}

void GSpiderMonkeyScriptObject::bindRaw(const char * name, const GVariant & value)
{
	ENTER_SPIDERMONKEY()

	LEAVE_SPIDERMONKEY()
}

void GSpiderMonkeyScriptObject::bindMethod(const char * name, void * instance, IMetaMethod * method)
{
	ENTER_SPIDERMONKEY()

	if(method->isStatic()) {
		instance = NULL;
	}

	GScopedInterface<IMetaList> methodList(createMetaList());
	methodList->add(method, instance);

	doBindMethodList(this->getSpiderContext(), GClassGlueDataPointer(), this->jsObject, name, methodList.get(), gdmtMethod);

	LEAVE_SPIDERMONKEY()
}

void GSpiderMonkeyScriptObject::bindMethodList(const char * name, IMetaList * methodList)
{
	ENTER_SPIDERMONKEY()

	doBindMethodList(this->getSpiderContext(), GClassGlueDataPointer(), this->jsObject, name, methodList, gdmtMethodList);

	LEAVE_SPIDERMONKEY()
}

IMetaClass * GSpiderMonkeyScriptObject::getClass(const char * className)
{
	ENTER_SPIDERMONKEY()

	return NULL;

	LEAVE_SPIDERMONKEY(return NULL)
}

IMetaEnum * GSpiderMonkeyScriptObject::getEnum(const char * enumName)
{
	ENTER_SPIDERMONKEY()

	return NULL;

	LEAVE_SPIDERMONKEY(return NULL)
}

GVariant GSpiderMonkeyScriptObject::getFundamental(const char * name)
{
	ENTER_SPIDERMONKEY()

	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, name, &value)) {
		if(getSpiderType(this->getSpiderContext(), value, NULL) == sdtFundamental) {
			return spiderToVariant(this->getSpiderContext(), value, NULL);
		}
	}

	return GVariant();

	LEAVE_SPIDERMONKEY(return GVariant())
}

std::string GSpiderMonkeyScriptObject::getString(const char * stringName)
{
	ENTER_SPIDERMONKEY()

	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, stringName, &value)) {
		if(value.isString()) {
			JSString * jsString = value.toString();
			GScopedArray<char> s(wideStringToString(JS_GetStringCharsZ(this->jsContext, jsString)));
			return s.get();
		}
	}

	return "";

	LEAVE_SPIDERMONKEY(return "")
}

void * GSpiderMonkeyScriptObject::getObject(const char * objectName)
{
	ENTER_SPIDERMONKEY()

	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, objectName, &value)) {
		return spiderToObject(this->jsContext, value);
	}

	return NULL;

	LEAVE_SPIDERMONKEY(return NULL)
}

GVariant GSpiderMonkeyScriptObject::getRaw(const char * name)
{
	ENTER_SPIDERMONKEY()

	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, name, &value)) {
		if(value.isObject()) {
			JSObject * object = &value.toObject();
			GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getObjectPrivateData(jsContext, object));
			if(dataWrapper != NULL && dataWrapper->getData()->getType() == sdtRaw) {
				return dataWrapper->getAs<GRawGlueData>()->getData();
			}
		}
	}

	return GVariant();

	LEAVE_SPIDERMONKEY(return GVariant())
}

IMetaMethod * GSpiderMonkeyScriptObject::getMethod(const char * methodName, void ** outInstance)
{
	ENTER_SPIDERMONKEY()

	if(outInstance != NULL) {
		*outInstance = NULL;
	}

	GMethodGlueDataPointer methodData = this->doGetMethodData(methodName);
	if(methodData && methodData->getMethodType() == gdmtMethod) {
		if(outInstance != NULL) {
			*outInstance = methodData->getMethodList()->getInstanceAt(0);
		}

		return gdynamic_cast<IMetaMethod *>(methodData->getMethodList()->getAt(0));
	}
	else {
		return NULL;
	}

	LEAVE_SPIDERMONKEY(return NULL)
}

IMetaList * GSpiderMonkeyScriptObject::getMethodList(const char * methodName)
{
	ENTER_SPIDERMONKEY()

	GMethodGlueDataPointer methodData = this->doGetMethodData(methodName);
	if(methodData && methodData->getMethodType() == gdmtMethodList) {
		methodData->getMethodList()->addReference();

		return methodData->getMethodList();
	}
	else {
		return NULL;
	}

	LEAVE_SPIDERMONKEY(return NULL)
}

void GSpiderMonkeyScriptObject::assignValue(const char * fromName, const char * toName)
{
	ENTER_SPIDERMONKEY()

	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, fromName, &value)) {
		JS_SetProperty(this->jsContext, this->jsObject, toName, &value);
	}

	LEAVE_SPIDERMONKEY()
}

bool GSpiderMonkeyScriptObject::valueIsNull(const char * name)
{
	ENTER_SPIDERMONKEY()

	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, name, &value)) {
		return value.isNullOrUndefined();
	}
	else {
		return false;
	}
	
	LEAVE_SPIDERMONKEY(return false)
}

void GSpiderMonkeyScriptObject::nullifyValue(const char * name)
{
	ENTER_SPIDERMONKEY()

	JsValue value = JSVAL_NULL;
	JS_SetProperty(this->jsContext, this->jsObject, name, &value);

	LEAVE_SPIDERMONKEY()
}

void GSpiderMonkeyScriptObject::bindCoreService(const char * name, IScriptLibraryLoader * libraryLoader)
{
	ENTER_SPIDERMONKEY()

	this->getContext()->bindScriptCoreService(this, name, libraryLoader);

	LEAVE_SPIDERMONKEY()
}

GMethodGlueDataPointer GSpiderMonkeyScriptObject::doGetMethodData(const char * methodName)
{
	JsValue value;
	if(JS_GetProperty(this->jsContext, this->jsObject, methodName, &value)) {
		if(value.isObject()) {
			JSObject * object = &value.toObject();
			GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(getFunctionPrivateData(jsContext, object));
			if(dataWrapper != NULL) {
				return dataWrapper->getAs<GMethodGlueData>();
			}
		}
	}

	return GMethodGlueDataPointer();
}


}


} // namespace cpgf

using namespace cpgf;

const char * greeting(int message)
{
	cout << "Hello, " << message << endl;
	return "that's fine";
}

class Point
{
public:
	Point() : x(0), y(0) {
	}

	Point(int x, int y) : x(x), y(y) {
	}

	void extend(int scale) {
		x *= scale;
		y *= scale;
	}

	int getArea() const {
		return x * y;
	}

	int getX() const {
		return x;
	}

	int getX2(int a) const {
		return x + a;
	}

public:
	int x;
	int y;
};

template <typename Define>
void reflectPoint(Define define)
{
	define._method("extend", &Point::extend);
	define._method("getArea", &Define::ClassType::getArea);
	define._method("getX", &Point::getX);
	define._method("getX", &Point::getX2);
	define.template _constructor<void * ()>();
	define.template _constructor<void * (int, int)>();
	define._field("x", &Point::x);
	define._field("y", &Point::y);
}

void buildMetaData()
{
	GDefineMetaClass<Point> defineForPoint = GDefineMetaClass<Point>::declare("Point");
	reflectPoint(defineForPoint);
	GDefineMetaGlobal defineForGlobal;
	defineForGlobal._method("greeting", &greeting);
	defineForGlobal._class(defineForPoint);
}

void testSpidermonkey()
{
	buildMetaData();

	GScopedInterface<IMetaService> service(createDefaultMetaService());

	GScopedInterface<IMetaMethod> method;
	GScopedInterface<IMetaField> field;
	GScopedInterface<IMetaModule> globalModule(service->getModuleAt(0));
	GScopedInterface<IMetaClass> globalMetaClass(globalModule->getGlobalMetaClass());
	GScopedInterface<IMetaClass> pointMetaClass(service->findClassByName("Point"));

	method.reset(globalMetaClass->getMethod("greeting"));
	
	JSRuntime *runtime=NULL;
	JSContext *jsContext=NULL;
	JSObject  *global=NULL;

	runtime = JS_NewRuntime(1024L*1024L, JS_NO_HELPER_THREADS);
	jsContext = JS_NewContext(runtime, 8192);
	global  = JS_NewObject(jsContext, NULL, NULL, NULL);
	
	if (!JS_InitStandardClasses(jsContext, global))
		return;

	GSpiderMonkeyScriptObject scriptObject(NULL, GScriptConfig(), jsContext, global);
	scriptObject.bindFundamental("vI", 38);
	scriptObject.bindString("vS", "What");
	scriptObject.bindMethod("hello", NULL, method.get());
	scriptObject.bindClass("Point", pointMetaClass.get());

	char *script=""
		"function abc(a) { return a + 5; } \n"
		"var vNull = null;"
		"var pt1 = new Point(1, 2);"
		"var pt2 = new Point(21, 22);"
		"pt1.x = 5;"
		"\"\" + pt1.getX(1) + \"  \" + pt2.x + \" -- \" + vS;"
	;
	jsval rval;

	JS_EvaluateScript(jsContext, global, script, (unsigned int)strlen(script), "script", 1, &rval);
	GScopedInterface<IMetaList> methodList();
	GScopedInterface<IMetaMethod> methodHello(scriptObject.getMethod("hello", NULL));
	cout << methodHello.get() << endl;

	if(JSVAL_IS_STRING(rval)) {
		JSString * jsString = rval.toString();
		const jschar * ws = JS_GetStringCharsZ(jsContext, jsString);
		GScopedArray<char> s(wideStringToString(ws));
		cout << "Result: " << s.get() << endl;
	}
	else {
		cout << "Result is wrong!" << endl;
	}
	GVariant v;
	GVariant parameters[1];
	parameters[0] = 2;
	v = scriptObject.invoke("abc", parameters, 1);
	cout << "abc= " << fromVariant<int>(v) << endl;

	JS_DestroyContext(jsContext);
	JS_DestroyRuntime(runtime);
	JS_ShutDown();
return;
	int s;
	cin >> s;
}