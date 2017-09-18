#include "stdafx.h"
#include "signal.hpp"
#include <cassert>
#include <cstdio>
#include <ctime>
#include <tchar.h>

Signal<void(int)> g_sig;

int g_call_count = 0;

struct cunk{};
struct ctest : Trackable
{
	void f1(int v)
	{
		printf("f1=%d\n", v);
		++g_call_count;
	}
	void f2(int v, const cunk&){
		// bind user param
		printf("%s=%d\n", __FUNCTION__, v);
		++g_call_count;
	}
	void f3_r(int v, const ctest&){
		printf("%s=%d\n", __FUNCTION__, v);
		++g_call_count;
	}
	void f3_p(int v, ctest*){
		printf("%s=%d\n", __FUNCTION__, v);
		++g_call_count;
	}

};
struct ctest_scope : Trackable
{
	ctest_scope(){
		g_sig.connect_member(&ctest_scope::fscope, this);
	}
	void fscope(int v){
		printf("fscope=%d\n", v);
		++g_call_count;
	}
};
void test0(){
	printf("%s\n", __FUNCTION__);
	++g_call_count;
}
void test1(int v){
	printf("%s=%d\n", __FUNCTION__, v);
	++g_call_count;
}
void test2(int a, int b){
	printf("%s=%d,%d\n", __FUNCTION__, a, b);
	++g_call_count;
}
void todo()
{
	{
		// test function class
		g_sig.connect(&test1);
		g_sig.connect(&test1, std::placeholders::_1);
		g_sig.connect(std::bind(&test1, std::placeholders::_1));
		//g_sig.connect(&test0); // error
		g_sig.connect(std::bind(&test0));
		g_sig.connect(&test2, std::placeholders::_1, 888);
		g_sig.connect(std::bind(&test2, std::placeholders::_1, 888));
		g_sig(101);
		assert(g_call_count == 6);
		g_call_count = 0;
		g_sig.disconnect_all();
		printf("=======\n");
	}

	{
		// test object weak lookup
		ctest* a = new ctest;
		g_sig.connect(&ctest::f1, a, std::placeholders::_1);
		g_sig.connect(&ctest::f2, a, std::placeholders::_1, cunk());
		//g_sig.connect(std::bind(&ctest::f1, a, std::placeholders::_1)); // [WARNING]lose weak
		g_sig(201);
		delete a;
		g_sig(202);
		assert(g_call_count == 2);
		g_call_count = 0;
		g_sig.disconnect_all();
	}
	{
		std::decay<int*>::type a = 0;
		// test scope weak
		ctest b;
		{
			ctest c;
			g_sig.connect(&ctest::f3_p, &b, std::placeholders::_1, &c);
			g_sig.connect(&ctest::f3_r, &b, std::placeholders::_1, std::ref(c));
			//g_sig.connect(&ctest::f3_r, &b, std::placeholders::_1, c); // [WARNING]'c' will clone to connect
			g_sig(301);
		}
		g_sig(302);
		assert(g_call_count == 2);
		g_call_count = 0;
		g_sig.disconnect_all();
		printf("=======\n");
	}

	{
		// test object class scope bind
		{
			ctest_scope a;
			g_sig(401);
		}
		g_sig(402);
		assert(g_call_count == 1);
		g_call_count = 0;
		g_sig.disconnect_all();
		printf("=======\n");

	}

	{
		// test lambda
		{
			int cid;
			{
				auto lam1 = [](int v){
					printf("lambda=%d\n", v);
					++g_call_count;
				};
				cid = g_sig.connect(lam1); // [WARNING]clone obj. MUST manual disconnect
				g_sig(501);
			}
			g_sig(502);
			g_sig.disconnect(cid);
			g_sig(503);
		}
		assert(g_call_count == 2);
		g_call_count = 0;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	srand((int)time(0));
	todo();

	system("pause");
	return 0;
}

