#include "cpgf/scriptbind/gscriptbind.h"
#include "cpgf/gmetaclasstraveller.h"
#include "cpgf/gstringmap.h"
#include "../../pinclude/gscriptbindapiimpl.h"

#include "v8.h"

#include "gbindcommon_new.h"

using namespace std;
using namespace cpgf::_bind_internal;
using namespace v8;


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif


#define ENTER_V8() \
	char local_msg[256]; bool local_error = false; { \
	try {

#define LEAVE_V8(...) \
	} \
	catch(const GException & e) { strncpy(local_msg, e.getMessage(), 256); local_error = true; } \
	catch(...) { strcpy(local_msg, "Unknown exception occurred."); local_error = true; } \
	} if(local_error) { local_msg[255] = 0; error(local_msg); } \
	__VA_ARGS__;


namespace cpgf {

namespace {


//*********************************************
// Declarations
//*********************************************

class GV8BindingContext : public GBindingContext
{
private:
	typedef GBindingContext super;

public:
	GV8BindingContext(IMetaService * service, const GScriptConfig & config)
		: super(service, config)
	{
	}

	virtual ~GV8BindingContext() {
		if(! this->objectTemplate.IsEmpty()) {
			this->objectTemplate.Dispose();
			this->objectTemplate.Clear();
		}
	}

	Handle<Object > getRawObject() {
		if(this->objectTemplate.IsEmpty()) {
			this->objectTemplate  = Persistent<ObjectTemplate>::New(ObjectTemplate::New());
			this->objectTemplate->SetInternalFieldCount(1);
		}

		return this->objectTemplate->NewInstance();
	}

private:
	Persistent<ObjectTemplate> objectTemplate;
};

class GV8ScriptObject : public GScriptObject
{
private:
	typedef GScriptObject super;

public:
	GV8ScriptObject(IMetaService * service, Local<Object> object, const GScriptConfig & config);
	virtual ~GV8ScriptObject();

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

	virtual GScriptObject * createScriptObject(const char * name);
	virtual GScriptObject * gainScriptObject(const char * name);

	virtual GScriptFunction * gainScriptFunction(const char * name);

	virtual GMetaVariant invoke(const char * name, const GMetaVariant * params, size_t paramCount);
	virtual GMetaVariant invokeIndirectly(const char * name, GMetaVariant const * const * params, size_t paramCount);

	virtual void assignValue(const char * fromName, const char * toName);
	virtual bool valueIsNull(const char * name);
	virtual void nullifyValue(const char * name);

	virtual void bindAccessible(const char * name, void * instance, IMetaAccessible * accessible);

	virtual void bindCoreService(const char * name);

public:
	const GContextPointer & getContext() const {
		return this->context;
	}

	Local<Object> getObject() const {
		return Local<Object>::New(this->object);
	}

private:
	GV8ScriptObject(const GV8ScriptObject & other, Local<Object> object);

private:
	GContextPointer context;
	Persistent<Object> object;
};

class GV8ScriptFunction : public GScriptFunction
{
public:
	GV8ScriptFunction(const GContextPointer & context, Local<Object> receiver, Local<Value> func);
	virtual ~GV8ScriptFunction();

	virtual GMetaVariant invoke(const GMetaVariant * params, size_t paramCount);
	virtual GMetaVariant invokeIndirectly(GMetaVariant const * const * params, size_t paramCount);

private:
	GWeakContextPointer context;
	Persistent<Object> receiver;
	Persistent<Function> func;
};

class GFunctionTemplateUserData : public GUserData
{
public:
	explicit GFunctionTemplateUserData(Handle<FunctionTemplate> functionTemplate)
		: functionTemplate(Persistent<FunctionTemplate>::New(functionTemplate))
	{
	}

	virtual ~GFunctionTemplateUserData() {
		this->functionTemplate.Dispose();
		this->functionTemplate.Clear();
	}

	Local<FunctionTemplate> getFunctionTemplate() const {
		return Local<FunctionTemplate>::New(this->functionTemplate);
	}

private:
	Persistent<FunctionTemplate> functionTemplate;
};


Handle<Value> variantToV8(const GContextPointer & context, const GVariant & value, const GMetaType & type, bool allowGC, bool allowRaw);
Handle<FunctionTemplate> createClassTemplate(const GContextPointer & context, const GClassGlueDataPointer & classData);


//*********************************************
// Global function implementations
//*********************************************


void error(const char * message)
{
	ThrowException(String::New(message));
}

void weakHandleCallback(Persistent<Value> object, void * parameter)
{
	GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(parameter);

	freeGlueDataPointer(dataWrapper);

	object.Dispose();
	object.Clear();
}

const char * signatureKey = "i_sig_cpgf";
const int signatureValue = 0x168feed;
const char * userDataKey = "i_userdata_cpgf";

template <typename T>
void setObjectSignature(T * object)
{
	(*object)->SetHiddenValue(String::New(signatureKey), Int32::New(signatureValue));
}

bool isValidObject(Handle<Value> object)
{
	if(object->IsObject() || object->IsFunction()) {
		Handle<Value> value = Handle<Object>::Cast(object)->GetHiddenValue(String::New(signatureKey));

		return !value.IsEmpty() && value->IsInt32() && value->Int32Value() == signatureValue;
	}
	else {
		return false;
	}
}

bool isGlobalObject(Handle<Value> object)
{
	if(object->IsObject() || object->IsFunction()) {
		return Handle<Object>::Cast(object)->InternalFieldCount () == 0
			|| Handle<Object>::Cast(object)->GetPointerFromInternalField(0) == NULL;
	}
	else {
		return false;
	}
}

GScriptDataType getV8Type(Local<Value> value, IMetaTypedItem ** typeItem)
{
	if(typeItem != NULL) {
		*typeItem = NULL;
	}

	if(value->IsNull()) {
		return sdtNull;
	}

	if(value->IsUndefined()) {
		return sdtNull;
	}

	if(value->IsBoolean()) {
		return sdtFundamental;
	}

	if(value->IsInt32() || value->IsNumber()) {
		return sdtFundamental;
	}

	if(value->IsString()) {
		return sdtString;
	}

	if(value->IsFunction() || value->IsObject()) {
		Local<Object> obj = value->ToObject();
		if(isValidObject(obj)) {
			if(obj->InternalFieldCount() == 0) {
				Handle<Value> value = obj->GetHiddenValue(String::New(userDataKey));
				if(! value.IsEmpty()) {
					if(value->IsExternal()) {
						GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(Handle<External>::Cast(value)->Value());
						switch(dataWrapper->getData()->getType()) {
							case gdtClass:
								if(typeItem != NULL) {
									*typeItem = dataWrapper->getAs<GClassGlueData>()->getMetaClass();
									(*typeItem)->addReference();
								}
								return sdtClass;

							case gdtExtendMethod:
//								return methodTypeToUserDataType(gdynamic_cast<GMethodUserData *>(userData)->getMethodData().getMethodType());

							default:
								break;
						}
					}
				}

			}
/*			else {
				GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(obj->GetPointerFromInternalField(0));
				if(dataWrapper != NULL) {
					switch(dataWrapper->getData()->getType()) {
						case gdtObject: {
							GObjectUserData * classData = gdynamic_cast<GObjectUserData *>(userData);
							if(typeItem != NULL) {
								*typeItem = classData->getObjectData()->getMetaClass();
								(*typeItem)->addReference();
							}
							if(classData->getObjectData()->getInstance() == NULL) {
								return sdtClass;
							}
							else {
								return sdtObject;
							}
						}

						break;

						case udtEnum:
							if(typeItem != NULL) {
								*typeItem = gdynamic_cast<GEnumUserData *>(userData)->getMetaEnum();
								(*typeItem)->addReference();
							}
							return sdtEnum;

					    case udtRaw:
					    	return sdtRaw;

						default:
						break;
					}
				}
			}
*/
		}

		if(value->IsFunction()) {
			return sdtScriptMethod;
		}
		else {
			return sdtScriptObject;
		}
	}

	return sdtUnknown;
}

GMetaVariant v8UserDataToVariant(const GContextPointer & context, Local<Context> v8Context, Handle<Value> value)
{
	if(value->IsFunction() || value->IsObject()) {
		Local<Object> obj = value->ToObject();
		if(isValidObject(obj)) {
			GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(obj->GetPointerFromInternalField(0));
			GGlueDataPointer glueData = dataWrapper->getData();
			return glueDataToVariant(glueData);
		}
		else {
			if(value->IsFunction()) {
				GScopedInterface<IScriptFunction> func(new ImplScriptFunction(new GV8ScriptFunction(context, v8Context->Global(), Local<Value>::New(value)), true));

				return GMetaVariant(func.get(), GMetaType());
			}
			else {
				GScopedInterface<IScriptObject> scriptObject(new ImplScriptObject(new GV8ScriptObject(context->getService(), obj, context->getConfig()), true));

				return GMetaVariant(scriptObject.get(), GMetaType());
			}
		}
	}

	return GMetaVariant();
}

GMetaVariant v8ToVariant(const GContextPointer & context, Local<Context> v8Context, Handle<Value> value)
{
	if(value.IsEmpty()) {
		return GMetaVariant();
	}

	if(value->IsBoolean()) {
		return value->BooleanValue();
	}

	if(value->IsInt32()) {
		return value->Int32Value();
	}

	if(value->IsNull()) {
		return (void *)0;
	}

	if(value->IsNumber()) {
		return value->NumberValue();
	}

	if(value->IsString()) {
		String::AsciiValue s(value);
		return GMetaVariant(createStringVariant(*s), createMetaType<char *>());
	}

	if(value->IsUint32()) {
		return value->Uint32Value();
	}

	if(value->IsFunction() || value->IsObject()) {
		return v8UserDataToVariant(context, v8Context, value);
	}

	return GMetaVariant();
}

Handle<Value> objectToV8(const GContextPointer & context, const GClassGlueDataPointer & classData, void * instance, bool allowGC, ObjectPointerCV cv, ObjectGlueDataType dataType)
{
	if(instance == NULL) {
		return Handle<Value>();
	}

	Handle<FunctionTemplate> functionTemplate = gdynamic_cast<GFunctionTemplateUserData *>(classData->getUserData())->getFunctionTemplate();
	Handle<Value> external = External::New(&signatureKey);
	Persistent<Object> self = Persistent<Object>::New(functionTemplate->GetFunction()->NewInstance(1, &external));

	GObjectGlueDataPointer objectData(context->newObjectGlueData(context, classData, instance, allowGC, cv, dataType));
	GGlueDataWrapper * dataWrapper = newGlueDataWrapper(objectData);
	self.MakeWeak(dataWrapper, weakHandleCallback);

	self->SetPointerInInternalField(0, dataWrapper);
	setObjectSignature(&self);

	return self;
}

Handle<Value> rawToV8(const GContextPointer & context, const GVariant & value)
{
	if(context->getConfig().allowAccessRawData()) {
		Persistent<Object> self = Persistent<Object>::New(sharedStaticCast<GV8BindingContext>(context)->getRawObject());

		GRawGlueDataPointer rawData(context->newRawGlueData(context, value));
		GGlueDataWrapper * dataWrapper = newGlueDataWrapper(rawData);
		self.MakeWeak(dataWrapper, weakHandleCallback);

		self->SetPointerInInternalField(0, dataWrapper);
		setObjectSignature(&self);

		return self;
	}

	return Handle<Value>();
}

struct GV8Methods
{
	typedef Handle<Value> ResultType;
	
	static ResultType doObjectToScript(const GContextPointer & context, const GClassGlueDataPointer & classData, void * instance, bool allowGC, ObjectPointerCV cv, ObjectGlueDataType dataType)
	{
		return objectToV8(context, classData, instance, allowGC, cv, dataType);
	}

	static ResultType doVariantToScript(const GContextPointer & context, const GVariant & value, const GMetaType & type, bool allowGC, bool allowRaw)
	{
		return variantToV8(context, value, type, allowGC, allowRaw);
	}
	
	static ResultType doRawToScript(const GContextPointer & context, const GVariant & value)
	{
		return rawToV8(context, value);
	}

	static ResultType doConverterToScript(const GContextPointer & context, const GVariant & value, IMetaConverter * converter)
	{
		ResultType result;
		if(converterToScript<GV8Methods>(&result, context, value, converter)) {
			return result;
		}
		return Handle<Value>();
	}

	static ResultType doClassToScript(const GContextPointer & context, IMetaClass * metaClass)
	{
		Handle<FunctionTemplate> functionTemplate = createClassTemplate(context, context->newClassGlueData(context, metaClass));
		return functionTemplate->GetFunction();
	}

	static ResultType doStringToScript(const GContextPointer & /*context*/, const char * s)
	{
		return String::New(s);
	}

	static ResultType doWideStringToScript(const GContextPointer & /*context*/, const wchar_t * ws)
	{
		GScopedArray<char> s(wideStringToString(ws));
		return String::New(s.get());
	}

	static bool isSuccessResult(const ResultType & result)
	{
		return ! result.IsEmpty();
	}
};

Handle<Value> variantToV8(const GContextPointer & context, const GVariant & value, const GMetaType & type, bool allowGC, bool allowRaw)
{
	GVariantType vt = static_cast<GVariantType>(value.getType() & ~byReference);

	if(vtIsEmpty(vt)) {
		return Handle<Value>();
	}

	if(vtIsBoolean(vt)) {
		return Boolean::New(fromVariant<bool>(value));
	}

	if(vtIsInteger(vt)) {
		return Integer::New(fromVariant<int>(value));
	}

	if(vtIsReal(vt)) {
		return Number::New(fromVariant<double>(value));
	}

	if(!vtIsInterface(vt) && canFromVariant<void *>(value) && objectAddressFromVariant(value) == NULL) {
		return Null();
	}

	if(variantIsString(value)) {
		return String::New(fromVariant<char *>(value));
	}

	if(variantIsWideString(value)) {
		const wchar_t * ws = fromVariant<wchar_t *>(value);
		GScopedArray<char> s(wideStringToString(ws));
		return String::New(s.get());
	}

	Handle<Value> result;
	if(variantToScript<GV8Methods>(&result, context, value, type, allowGC, allowRaw)) {
		return result;
	}

	return Handle<Value>();
}

Handle<Value> getNamedMember(const GGlueDataPointer & glueData, const char * name)
{
	bool isInstance = (glueData->getType() == gdtObject);
	GClassGlueDataPointer classData;
	GObjectGlueDataPointer objectData;
	if(glueData->getType() == gdtObject) {
		objectData = sharedStaticCast<GObjectGlueData>(glueData);
		classData = objectData->getClassData();
	}
	else {
		GASSERT(glueData->getType() == gdtClass);
		classData = sharedStaticCast<GClassGlueData>(glueData);
	}

	const GScriptConfig & config = classData->getContext()->getConfig();
	GContextPointer context = classData->getContext();

	GMetaClassTraveller traveller(classData->getMetaClass(), getGlueDataInstance(glueData));

	void * instance = NULL;
	IMetaClass * outDerived;

	for(;;) {
		GScopedInterface<IMetaClass> metaClass(traveller.next(&instance, &outDerived));
		GScopedInterface<IMetaClass> derived(outDerived);

		if(!metaClass) {
			break;
		}

		GMetaMapClassPointer mapClass = classData->getClassMap();
		if(! mapClass) {
			continue;
		}

		GMetaMapItem * mapItem = mapClass->findItem(name);
		if(mapItem == NULL) {
			continue;
		}

		switch(mapItem->getType()) {
			case mmitField:
			case mmitProperty: {
				GScopedInterface<IMetaAccessible> data(gdynamic_cast<IMetaAccessible *>(mapItem->getItem()));
				if(allowAccessData(config, isInstance, data.get())) {
					Handle<Value> r;
					if(accessibleToScript<GV8Methods>(&r, context, data.get(), instance, getGlueDataCV(glueData) == opcvConst)) {
						return r;
					}
					return Handle<Value>();
				}
			}
			   break;

/*
			case mmitMethod:
			case mmitMethodList: {
				GMapItemV8MethodData * data = gdynamic_cast<GMapItemV8MethodData *>(mapItem->getData());
				if(data == NULL) {
					GScopedInterface<IMetaList> methodList(createMetaList());
					loadMethodList(&traveller, methodList.get(), userData->getParam()->getMetaMap(), mapItem, instance, name);

					data = new GMapItemV8MethodData;
					mapItem->setData(data);
					GMethodUserData * newUserData;

					GScopedInterface<IMetaClass> boundClass(selectBoundClass(metaClass.get(), derived.get()));

					GMetaMapClass * baseMapClass = getMetaClassMap(userData->getParam(), boundClass.get());
					data->setTemplate(createMethodTemplate(userData->getParam(), userData->getObjectData()->getMetaClass(),
						userData->getObjectData()->getInstance() == NULL, methodList.get(), name,
						gdynamic_cast<GMapItemClassData *>(baseMapClass->getData())->getFunctionTemplate(), udmtInternal, &newUserData));
					data->setUserData(newUserData);
				}

				return data->functionTemplate->GetFunction();
			}

			case mmitEnum:
				if(! userData->getObjectData()->isInstance() || config.allowAccessEnumTypeViaInstance()) {
					GMapItemEnumData * data = gdynamic_cast<GMapItemEnumData *>(mapItem->getData());
					if(data == NULL) {
						data = new GMapItemEnumData;
						mapItem->setData(data);
						GEnumUserData * newUserData;
						GScopedInterface<IMetaEnum> metaEnum(gdynamic_cast<IMetaEnum *>(mapItem->getItem()));
						data->setTemplate(createEnumTemplate(userData->getParam(), metaEnum.get(), name, &newUserData));
						data->setUserData(newUserData);
					}
					Local<Object> obj = data->objectTemplate->NewInstance();
					obj->SetPointerInInternalField(0, data->getUserData());
					setObjectSignature(&obj);
					return obj;
				}
				break;

			case mmitEnumValue:
				if(! userData->getObjectData()->isInstance() || config.allowAccessEnumValueViaInstance()) {
					GScopedInterface<IMetaEnum> metaEnum(gdynamic_cast<IMetaEnum *>(mapItem->getItem()));
					return variantToV8(userData->getParam(), metaGetEnumValue(metaEnum, static_cast<uint32_t>(mapItem->getEnumIndex())), GMetaType(), false, true);
				}
				break;
*/
			case mmitClass:
				if(! isInstance || config.allowAccessClassViaInstance()) {
					GScopedInterface<IMetaClass> innerMetaClass(gdynamic_cast<IMetaClass *>(mapItem->getItem()));
					Handle<FunctionTemplate> functionTemplate = createClassTemplate(context, context->requireClassGlueData(context, innerMetaClass.get()));
					return functionTemplate->GetFunction();
				}
				break;

			default:
				break;
		}
	}

	return Handle<Value>();
}

void loadMethodParameters(const Arguments & args, const GContextPointer & context, GVariant * outputParams)
{
	for(int i = 0; i < args.Length(); ++i) {
		outputParams[i] = v8ToVariant(context, args.Holder()->CreationContext(), args[i]).getValue();
	}
}

void loadMethodParamTypes(const Arguments & args, CallableParamDataType * outputTypes)
{
	for(int i = 0; i < args.Length(); ++i) {
		IMetaTypedItem * typeItem;
		outputTypes[i].dataType = getV8Type(args[i], &typeItem);
		outputTypes[i].typeItem.reset(typeItem);
	}
}

void loadCallableParam(const Arguments & args, const GContextPointer & context, InvokeCallableParam * callableParam)
{
	loadMethodParameters(args, context, callableParam->paramsData);
	loadMethodParamTypes(args, callableParam->paramsType);
}

void * invokeConstructor(const Arguments & args, const GContextPointer & context, IMetaClass * metaClass)
{
	InvokeCallableParam callableParam(args.Length());
	loadCallableParam(args, context, &callableParam);

	void * instance = doInvokeConstructor(context->getService(), metaClass, &callableParam);

	if(instance != NULL) {
		return instance;
	}
	else {
		raiseCoreException(Error_ScriptBinding_InternalError_WrongFunctor);
	}

	return NULL;
}

Handle<Value> objectConstructor(const Arguments & args)
{
	ENTER_V8()

	if(! args.IsConstructCall()) {
		return ThrowException(String::New("Cannot call constructor as function"));
	}

	Persistent<Object> self = Persistent<Object>::New(args.Holder());

	if(args.Length() == 1 && args[0]->IsExternal() && External::Unwrap(args[0]) == &signatureKey) {
		// Here means this constructor is called when wrapping an existing object, so we don't create new object.
		// See function objectToV8
	}
	else {
		Local<External> data = Local<External>::Cast(args.Data());
		GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(data->Value());
		GClassGlueDataPointer classData = dataWrapper->getAs<GClassGlueData>();

		void * instance = invokeConstructor(args, classData->getContext(), classData->getMetaClass());

		GObjectGlueDataPointer objectData = classData->getContext()->newObjectGlueData(classData->getContext(), classData, instance, true, opcvNone, ogdtNormal);
		GGlueDataWrapper * objectWrapper = newGlueDataWrapper(objectData);
		self.MakeWeak(objectWrapper, weakHandleCallback);

		self->SetPointerInInternalField(0, objectWrapper);
		setObjectSignature(&self);
	}

	return self;

	LEAVE_V8(return Handle<Value>());
}

Handle<Value> staticMemberGetter(Local<String> prop, const AccessorInfo & info)
{
	ENTER_V8()

	GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(Local<External>::Cast(info.Data())->Value());

	String::Utf8Value utf8_prop(prop);
	const char * name = *utf8_prop;

	return getNamedMember(dataWrapper->getData(), name);

	LEAVE_V8(return Handle<Value>())
}

void staticMemberSetter(Local<String> prop, Local<Value> value, const AccessorInfo & info)
{
	ENTER_V8()

	GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(Local<External>::Cast(info.Data())->Value());

	String::Utf8Value utf8_prop(prop);
	const char * name = *utf8_prop;

	GContextPointer context = dataWrapper->getData()->getContext();

	doSetFieldValue(dataWrapper->getData(), name, v8ToVariant(context, info.Holder()->CreationContext(), value).getValue());

	LEAVE_V8()
}

Handle<Value> namedMemberGetter(Local<String> prop, const AccessorInfo & info)
{
	ENTER_V8()

	if(!isValidObject(info.Holder())) {
		raiseCoreException(Error_ScriptBinding_AccessMemberWithWrongObject);
	}

	String::Utf8Value utf8_prop(prop);
	const char * name = *utf8_prop;

	GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(info.Holder()->GetPointerFromInternalField(0));

	return getNamedMember(dataWrapper->getData(), name);

	LEAVE_V8(return Handle<Value>())
}

Handle<Value> namedMemberSetter(Local<String> prop, Local<Value> value, const AccessorInfo & info)
{
	ENTER_V8()

	String::Utf8Value utf8_prop(prop);
	const char * name = *utf8_prop;

	if(!isValidObject(info.Holder())) {
		raiseCoreException(Error_ScriptBinding_AccessMemberWithWrongObject);
	}

	GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(info.Holder()->GetPointerFromInternalField(0));

	if(getGlueDataCV(dataWrapper->getData()) == opcvConst) {
		raiseCoreException(Error_ScriptBinding_CantWriteToConstObject);

		return Handle<Value>();
	}

	if(doSetFieldValue(dataWrapper->getData(), name, v8ToVariant(dataWrapper->getData()->getContext(), info.Holder()->CreationContext(), value).getValue())) {
		return value;
	}

	return Handle<Value>();

	LEAVE_V8(return Handle<Value>())
}

Handle<Array> namedMemberEnumerator(const AccessorInfo & info)
{
	ENTER_V8()

	if(!isValidObject(info.Holder())) {
		raiseCoreException(Error_ScriptBinding_AccessMemberWithWrongObject);
	}

	GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(info.Holder()->GetPointerFromInternalField(0));
	GGlueDataPointer glueData = dataWrapper->getData();

	GMetaClassTraveller traveller(getGlueDataMetaClass(glueData), getGlueDataInstance(glueData));
	GStringMap<bool, GStringMapReuseKey> nameMap;
	GScopedInterface<IMetaItem> metaItem;

	for(;;) {
		GScopedInterface<IMetaClass> metaClass(traveller.next(NULL, NULL));

		if(!metaClass) {
			break;
		}

		uint32_t metaCount = metaClass->getMetaCount();
		for(uint32_t i = 0; i < metaCount; ++i) {
			metaItem.reset(metaClass->getMetaAt(i));
			nameMap.set(metaItem->getName(), true);
		}
	}

	HandleScope handleScope;

	Local<Array> metaNames = Array::New(nameMap.getCount());
	int i = 0;
	for(GStringMap<bool, GStringMapReuseKey>::iterator it = nameMap.begin(); it != nameMap.end(); ++it) {
		metaNames->Set(Number::New(i), String::New(it->first));
		++i;
	}

	return handleScope.Close(metaNames);

	LEAVE_V8(return Handle<Array>())
}

void bindClassItems(Local<Object> object, IMetaClass * metaClass, Persistent<External> objectData)
{
	GScopedInterface<IMetaItem> item;
	uint32_t count = metaClass->getMetaCount();
	for(uint32_t i = 0; i < count; ++i) {
		item.reset(metaClass->getMetaAt(i));
		if(item->isStatic()) {
			object->SetAccessor(String::New(item->getName()), &staticMemberGetter, &staticMemberSetter, objectData);
			if(metaIsEnum(item->getCategory())) {
				IMetaEnum * metaEnum = gdynamic_cast<IMetaEnum *>(item.get());
				uint32_t keyCount = metaEnum->getCount();
				for(uint32_t k = 0; k < keyCount; ++k) {
					object->SetAccessor(String::New(metaEnum->getKey(k)), &staticMemberGetter, &staticMemberSetter, objectData);
				}
			}
		}
		else {
			// to allow override method with script function
			if(metaIsMethod(item->getCategory())) {
				object->SetAccessor(String::New(item->getName()), NULL, &staticMemberSetter, objectData);
			}
		}
	}
}

Handle<FunctionTemplate> doCreateClassTemplate(const GContextPointer & context, GGlueDataWrapper * dataWrapper, const GClassGlueDataPointer & classData)
{
	if(classData->getUserData() != NULL) {
		return gdynamic_cast<GFunctionTemplateUserData *>(classData->getUserData())->getFunctionTemplate();
	}

	IMetaClass * metaClass = classData->getMetaClass();

	Persistent<External> data = Persistent<External>::New(External::New(dataWrapper));
	data.MakeWeak(dataWrapper, weakHandleCallback);

	Handle<FunctionTemplate> functionTemplate = FunctionTemplate::New(objectConstructor, data);
	functionTemplate->SetClassName(String::New(metaClass->getName()));

	classData->setUserData(new GFunctionTemplateUserData(functionTemplate));

	Local<ObjectTemplate> instanceTemplate = functionTemplate->InstanceTemplate();
	instanceTemplate->SetInternalFieldCount(1);

	instanceTemplate->SetNamedPropertyHandler(&namedMemberGetter, &namedMemberSetter, NULL, NULL, &namedMemberEnumerator);

	if(metaClass->getBaseCount() > 0) {
		GScopedInterface<IMetaClass> baseClass(metaClass->getBaseClass(0));
		if(baseClass) {
			GClassGlueDataPointer baseClassData = context->requireClassGlueData(context, baseClass.get());
			GGlueDataWrapper * baseDataWrapper = newGlueDataWrapper(baseClassData);
			Handle<FunctionTemplate> baseFunctionTemplate = doCreateClassTemplate(context, baseDataWrapper, baseClassData);
			functionTemplate->Inherit(baseFunctionTemplate);
		}
	}

	Local<Function> classFunction = functionTemplate->GetFunction();
	setObjectSignature(&classFunction);
	bindClassItems(classFunction, metaClass, data);

	classFunction->SetHiddenValue(String::New(userDataKey), data);

	return functionTemplate;
}

Handle<FunctionTemplate> createClassTemplate(const GContextPointer & context, const GClassGlueDataPointer & classData)
{
	GGlueDataWrapper * dataWrapper = newGlueDataWrapper(classData);
	return doCreateClassTemplate(context, dataWrapper, classData);
}

void doBindClass(const GContextPointer & context, Local<Object> container, const char * name, IMetaClass * metaClass)
{
	Handle<FunctionTemplate> functionTemplate = createClassTemplate(context, context->newClassGlueData(context, metaClass));
	container->Set(String::New(name), functionTemplate->GetFunction());
}

bool valueIsCallable(Local<Value> value)
{
	return value->IsFunction() || (value->IsObject() && Local<Object>::Cast(value)->IsCallable());
}

GMetaVariant invokeV8FunctionIndirectly(const GContextPointer & context, Local<Object> object, Local<Value> func, GMetaVariant const * const * params, size_t paramCount, const char * name)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");
/*
	if(! context) {
		raiseCoreException(Error_ScriptBinding_NoContext);
	}

	if(valueIsCallable(func)) {
		Handle<Value> v8Params[REF_MAX_ARITY];
		for(size_t i = 0; i < paramCount; ++i) {
			v8Params[i] = variantToV8(context, params[i]->getValue(), params[i]->getType(), false, true);
			if(v8Params[i].IsEmpty()) {
				raiseCoreException(Error_ScriptBinding_ScriptMethodParamMismatch, i, name);
			}
		}

		Local<Value> result;
		Handle<Object> receiver = object;

		if(func->IsFunction()) {
			result = Local<Function>::Cast(func)->Call(receiver, static_cast<int>(paramCount), v8Params);
		}
		else {
			result = Local<Object>::Cast(func)->CallAsFunction(receiver, static_cast<int>(paramCount), v8Params);
		}

		return v8ToVariant(context, receiver->CreationContext(), result);
	}
	else {
		raiseCoreException(Error_ScriptBinding_CantCallNonfunction);
	}
*/
	return GMetaVariant();
}


//*********************************************
// Classes implementations
//*********************************************

GV8ScriptFunction::GV8ScriptFunction(const GContextPointer & context, Local<Object> receiver, Local<Value> func)
	: context(context),
		receiver(Persistent<Object>::New(Local<Object>::Cast(receiver))),
		func(Persistent<Function>::New(Local<Function>::Cast(func)))
{
}

GV8ScriptFunction::~GV8ScriptFunction()
{
	this->receiver.Dispose();
	this->receiver.Clear();
	this->func.Dispose();
	this->func.Clear();
}

GMetaVariant GV8ScriptFunction::invoke(const GMetaVariant * params, size_t paramCount)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");

	const cpgf::GMetaVariant * variantPointers[REF_MAX_ARITY];

	for(size_t i = 0; i < paramCount; ++i) {
		variantPointers[i] = &params[i];
	}

	return this->invokeIndirectly(variantPointers, paramCount);
}

GMetaVariant GV8ScriptFunction::invokeIndirectly(GMetaVariant const * const * params, size_t paramCount)
{
	ENTER_V8()

	HandleScope handleScope;

	Local<Object> receiver = Local<Object>::New(this->receiver);
	return invokeV8FunctionIndirectly(this->context.get(), receiver, Local<Value>::New(this->func), params, paramCount, "");

	LEAVE_V8(return GMetaVariant())
}


GV8ScriptObject::GV8ScriptObject(IMetaService * service, Local<Object> object, const GScriptConfig & config)
	: super(config), context(new GV8BindingContext(service, config)), object(object)
{
}

GV8ScriptObject::GV8ScriptObject(const GV8ScriptObject & other, Local<Object> object)
	: super(other.context->getConfig()), context(other.context), object(object)
{
}

GV8ScriptObject::~GV8ScriptObject()
{
}

GScriptDataType GV8ScriptObject::getType(const char * name, IMetaTypedItem ** outMetaTypeItem)
{
	ENTER_V8()

	HandleScope handleScope;
	Local<Object> localObject(Local<Object>::New(this->object));

	Local<Value> obj = localObject->Get(String::New(name));
	// TBD
	//return getV8Type(obj, outMetaTypeItem);
	return sdtUnknown;

	LEAVE_V8(return sdtUnknown)
}

void GV8ScriptObject::bindClass(const char * name, IMetaClass * metaClass)
{
	ENTER_V8()

	HandleScope handleScope;
	Local<Object> localObject(Local<Object>::New(this->object));

	doBindClass(this->context, localObject, name, metaClass);

	LEAVE_V8()
}

void GV8ScriptObject::bindEnum(const char * name, IMetaEnum * metaEnum)
{
	ENTER_V8()


	LEAVE_V8()
}

GScriptObject * GV8ScriptObject::createScriptObject(const char * name)
{
	ENTER_V8()

	return NULL;

	LEAVE_V8(return NULL)
}

GScriptObject * GV8ScriptObject::gainScriptObject(const char * name)
{
	ENTER_V8()

	return NULL;

	LEAVE_V8(return NULL)
}

GScriptFunction * GV8ScriptObject::gainScriptFunction(const char * name)
{
	ENTER_V8()

	return NULL;

	LEAVE_V8(return NULL)
}

GMetaVariant GV8ScriptObject::invoke(const char * name, const GMetaVariant * params, size_t paramCount)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");

	const cpgf::GMetaVariant * variantPointers[REF_MAX_ARITY];

	for(size_t i = 0; i < paramCount; ++i) {
		variantPointers[i] = &params[i];
	}

	return this->invokeIndirectly(name, variantPointers, paramCount);
}

GMetaVariant GV8ScriptObject::invokeIndirectly(const char * name, GMetaVariant const * const * params, size_t paramCount)
{
	ENTER_V8()

	HandleScope handleScope;
	Local<Object> localObject(Local<Object>::New(this->object));

	Local<Value> func = localObject->Get(String::New(name));

	return invokeV8FunctionIndirectly(this->getContext(), this->getObject(), func, params, paramCount, name);

	LEAVE_V8(return GMetaVariant())
}

void GV8ScriptObject::bindFundamental(const char * name, const GVariant & value)
{
	GASSERT_MSG(vtIsFundamental(vtGetType(value.data.typeData)), "Only fundamental value can be bound via bindFundamental");

	ENTER_V8()

	LEAVE_V8()
}

void GV8ScriptObject::bindAccessible(const char * name, void * instance, IMetaAccessible * accessible)
{
	ENTER_V8()

	LEAVE_V8()
}

void GV8ScriptObject::bindString(const char * stringName, const char * s)
{
	ENTER_V8()

	LEAVE_V8()
}

void GV8ScriptObject::bindObject(const char * objectName, void * instance, IMetaClass * type, bool transferOwnership)
{
	ENTER_V8()

	LEAVE_V8()
}

void GV8ScriptObject::bindRaw(const char * name, const GVariant & value)
{
	ENTER_V8()

	LEAVE_V8()
}

void GV8ScriptObject::bindMethod(const char * name, void * instance, IMetaMethod * method)
{
	ENTER_V8()

	LEAVE_V8()
}

void GV8ScriptObject::bindMethodList(const char * name, IMetaList * methodList)
{
	ENTER_V8()

	LEAVE_V8()
}

IMetaClass * GV8ScriptObject::getClass(const char * className)
{
	return NULL;
}

IMetaEnum * GV8ScriptObject::getEnum(const char * enumName)
{
	return NULL;
}

GVariant GV8ScriptObject::getFundamental(const char * name)
{
	ENTER_V8()

	return GVariant();

	LEAVE_V8(return GVariant())
}

std::string GV8ScriptObject::getString(const char * stringName)
{
	ENTER_V8()

	return "";

	LEAVE_V8(return "")
}

void * GV8ScriptObject::getObject(const char * objectName)
{
	ENTER_V8()

	return NULL;

	LEAVE_V8(return NULL)
}

GVariant GV8ScriptObject::getRaw(const char * name)
{
	ENTER_V8()

	return GVariant();

	LEAVE_V8(return GVariant())
}

IMetaMethod * GV8ScriptObject::getMethod(const char * methodName, void ** outInstance)
{
	ENTER_V8()

	return NULL;

	LEAVE_V8(return NULL)
}

IMetaList * GV8ScriptObject::getMethodList(const char * methodName)
{
	ENTER_V8()

	return NULL;

	LEAVE_V8(return NULL)
}

void GV8ScriptObject::assignValue(const char * fromName, const char * toName)
{
	ENTER_V8()

	LEAVE_V8()
}

bool GV8ScriptObject::valueIsNull(const char * name)
{
	ENTER_V8()

	LEAVE_V8(return false)
}

void GV8ScriptObject::nullifyValue(const char * name)
{
	ENTER_V8()

	LEAVE_V8()
}

void GV8ScriptObject::bindCoreService(const char * name)
{
	ENTER_V8()

	LEAVE_V8()
}




} // unnamed namespace


GScriptObject * new_createV8ScriptObject(IMetaService * service, Local<Object> object, const GScriptConfig & config)
{
	return new GV8ScriptObject(service, object, config);
}



} // namespace cpgf