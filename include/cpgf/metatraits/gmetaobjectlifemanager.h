#ifndef __GMETAOBJECTLIFEMANAGER_H
#define __GMETAOBJECTLIFEMANAGER_H

#include "cpgf/gapi.h"


namespace cpgf {

struct GMetaTraitsParam;
struct IMetaClass;

struct IMetaObjectLifeManager : public IObject
{
	virtual void G_API_CC retainObject(void * object) = 0;
	virtual void G_API_CC releaseObject(void * object) = 0;
	virtual void G_API_CC freeObject(void * object, IMetaClass * metaClass) = 0;
	virtual void G_API_CC returnedFromMethod(void * object) = 0;
};


namespace metatraits_internal {
	IMetaObjectLifeManager * createDefaultObjectLifeManagerFromMetaTraits();
} // namespace metatraits_internal


} // namespace cpgf


namespace cpgf_metatraits {

inline cpgf::IMetaObjectLifeManager * metaTraitsCreateObjectLifeManager(const cpgf::GMetaTraitsParam & /*param*/, ...)
{
	return NULL;
}

} // namespace cpgf_metatraits


namespace cpgf {

template <typename T, typename Enabled = void>
struct GMetaTraitsCreateObjectLifeManager
{
	static IMetaObjectLifeManager * createObjectLifeManager(const GMetaTraitsParam &) {
		return NULL;
	}
};

template <typename T>
IMetaObjectLifeManager * createObjectLifeManagerFromMetaTraits(const GMetaTraitsParam & param, T * p)
{
	using namespace cpgf_metatraits;

	IMetaObjectLifeManager * objectLifeManager = metaTraitsCreateObjectLifeManager(param, p);
	if(objectLifeManager == NULL) {
		objectLifeManager = GMetaTraitsCreateObjectLifeManager<T>::createObjectLifeManager(param);
	}
	if(objectLifeManager == NULL) {
		objectLifeManager = metatraits_internal::createDefaultObjectLifeManagerFromMetaTraits();
	}
	return objectLifeManager;
}

} // namespace cpgf


#endif