#include "operator_common.h"


namespace {

class TestObject
{
	DEF_TEST_OBJECT(TestObject)

public:
	TestObject & operator , (int n) {
		this->value += n;

		return *this;
	}

	std::string operator [] (int n) const {
		std::string s = "";

		while(n > 0) {
			--n;
			s = s + "a";
		}

		return s;
	}

	int * operator -> () {
		return &this->value;
	}

	int operator ->* (int TestObject::* p) {
		return this->*p;
	}

	operator long () const {
		return this->value + 1;
	}

	operator std::string () const {
		return "abc";
	}

	int operator () (const std::string & s, int n) const {
		return this->value + (int)s.length() + n * 2;
	}

	int operator () (const cpgf::GMetaVariadicParam * params) const {
		int total = 0;
		for(size_t i = 0; i < params->paramCount; ++i) {
			total += cpgf::fromVariant<int>(*(params->params[i]));
		}

		return total;
	}

};

GMETA_DEFINE_CLASS(TestObject, TestObject, "operator_other::TestObject") {
	using namespace cpgf;

	reflectOperator<TestObject & (GMetaSelf, int)>((mopHolder , mopHolder));

	reflectOperator<std::string (GMetaSelf, int)>(mopHolder[0]);

	reflectOperator<int * (GMetaSelf)>(mopHolder->mopHolder);

	reflectOperator<int (GMetaSelf, int TestObject::*)>(mopHolder->*mopHolder);

	reflectOperator<long (GMetaSelf)>(mopHolder());
	reflectOperator<std::string (GMetaSelf)>(mopHolder());

	reflectOperator<int (const std::string &, int)>(mopHolder(mopHolder));
	reflectOperator<int (const cpgf::GMetaVariadicParam *)>(mopHolder(mopHolder));
}

void doTestLib()
{
	using namespace cpgf;

	const GMetaClass * metaClass;
	const GMetaOperator * metaOperator;

	metaClass = findMetaClass("operator_other::TestObject");
	testCheckAssert(metaClass != NULL);
	std::cout << "Lib: " << metaClass->getName() << std::endl;

	{
		metaOperator = getOperator((mopHolder , mopHolder), metaClass, 0); testCheckAssert(metaOperator != NULL);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		TestObject res = fromVariant<TestObject>(metaOperator->invokeBinary(obj, 3));
		testCheckEqual(res, (back , 3));
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator = getOperator(mopHolder[0], metaClass, 0); testCheckAssert(metaOperator != NULL);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		std::string res = fromVariant<std::string>(metaOperator->invokeBinary(obj, 3));
		testCheckEqual(res, back[3]);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator = getOperator(mopHolder->mopHolder, metaClass, 0); testCheckAssert(metaOperator != NULL);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		int * pn = fromVariant<int *>(metaOperator->invokeUnary(obj));
		testCheckEqual(pn, &((TestObject *)obj)->value);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator = getOperator(mopHolder->*mopHolder, metaClass, 0); testCheckAssert(metaOperator != NULL);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		int TestObject::* memPtr = &TestObject::value;
		int n = fromVariant<int>(metaOperator->invokeBinary(obj, memPtr));
		testCheckEqual(n, back->*memPtr);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator = getOperator(mopHolder(), metaClass, 0); testCheckAssert(metaOperator != NULL);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		long n = fromVariant<int>(metaOperator->invokeUnary(obj));
		testCheckEqual(n, (long)back);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator = getOperator(mopHolder(), metaClass, 1); testCheckAssert(metaOperator != NULL);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		std::string s = fromVariant<std::string>(metaOperator->invokeUnary(obj));
		testCheckEqual(s, (std::string)back);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator = getOperator(mopHolder(mopHolder), metaClass, 0); testCheckAssert(metaOperator != NULL);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		int n = fromVariant<int>(metaOperator->invokeFunctor(obj, "abc", 5));
		testCheckEqual(n, back("abc", 5));
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator = getOperator(mopHolder(mopHolder), metaClass, 1); testCheckAssert(metaOperator != NULL);
		void * obj = metaClass->createInstance();
		int n = fromVariant<int>(metaOperator->invokeFunctor(obj, 1, 2, 3, 5, 6, 7, 8, 9));
		testCheckEqual(n, 1 + 2 + 3 + 5 + 6 + 7 + 8 + 9);
		metaClass->destroyInstance(obj);
	}


}

void doTestAPI()
{
	using namespace cpgf;

	GApiScopedPointer<IMetaService> service(createMetaService());
	testCheckAssert(service);

	GApiScopedPointer<IMetaClass> metaClass(service->findClassByName("operator_other::TestObject"));
	testCheckAssert(metaClass);

	std::cout << "API: " << metaClass->getName() << std::endl;

	GApiScopedPointer<IMetaOperator> metaOperator;

	{
		metaOperator.reset(getOperator((mopHolder , mopHolder), metaClass, 0)); testCheckAssert(metaOperator);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		TestObject res = fromVariant<TestObject>(metaCallOperatorBinary(metaOperator, obj, 3));
		testCheckEqual(res, (back , 3));
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator.reset(getOperator(mopHolder[0], metaClass, 0)); testCheckAssert(metaOperator);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		std::string res = fromVariant<std::string>(metaCallOperatorBinary(metaOperator, obj, 3));
		testCheckEqual(res, back[3]);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator.reset(getOperator(mopHolder->mopHolder, metaClass, 0)); testCheckAssert(metaOperator);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		int * pn = fromVariant<int *>(metaCallOperatorUnary(metaOperator, obj));
		testCheckEqual(pn, &((TestObject *)obj)->value);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator.reset(getOperator(mopHolder->*mopHolder, metaClass, 0)); testCheckAssert(metaOperator);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		int TestObject::* memPtr = &TestObject::value;
		int n = fromVariant<int>(metaCallOperatorBinary(metaOperator, obj, memPtr));
		testCheckEqual(n, back->*memPtr);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator.reset(getOperator(mopHolder(), metaClass, 0)); testCheckAssert(metaOperator);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		long n = fromVariant<int>(metaCallOperatorUnary(metaOperator, obj));
		testCheckEqual(n, (long)back);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator.reset(getOperator(mopHolder(), metaClass, 1)); testCheckAssert(metaOperator);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		std::string s = fromVariant<std::string>(metaCallOperatorUnary(metaOperator, obj));
		testCheckEqual(s, (std::string)back);
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator.reset(getOperator(mopHolder(mopHolder), metaClass, 0)); testCheckAssert(metaOperator);
		void * obj = metaClass->createInstance();
		TestObject back(*(TestObject *)obj);
		int n = fromVariant<int>(metaCallOperatorFunctor(metaOperator, obj, "abc", 5));
		testCheckEqual(n, back("abc", 5));
		metaClass->destroyInstance(obj);
	}

	{
		metaOperator.reset(getOperator(mopHolder(mopHolder), metaClass, 1)); testCheckAssert(metaOperator);
		void * obj = metaClass->createInstance();
		int n = fromVariant<int>(metaCallOperatorFunctor(metaOperator, obj, 1, 2, 3, 5, 6, 7, 8, 9));
		testCheckEqual(n, 1 + 2 + 3 + 5 + 6 + 7 + 8 + 9);
		metaClass->destroyInstance(obj);
	}


}



void doTest()
{
	doTestLib();
	doTestAPI();
}

gTestCase(doTest);


}
