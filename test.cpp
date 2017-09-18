#include "stdafx.h"
#include <cassert>
#include <cstdio>
#include <ctime>
#include <tchar.h>
#include "signal.hpp"
Signal<void(int)> g_sig;
//#define SIGNALS_ENABLE_REF_BIND 1

int g_call_count = 0;

struct cunk{};
struct ctest : Trackable
{
#if 0
	ctest() {
		printf("new\n");
	}
	ctest(const ctest& r) : Trackable(r) {
		printf("copy\n");
	}
	~ctest(){
		printf("delete\n");
	}
#endif
	void f1(int v){
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
void todo(bool test_warning = true)
{
	int normal_count = 0;
	int warning_count = 0;
	int cid;

	auto clear_sigal_state = [&](){
		g_call_count = 0;
		warning_count = 0;
		normal_count = 0;
		cid = 0;
		g_sig.disconnect_all();
		printf("=======\n");
	};

	{
		// test function class
		g_sig.connect(&test1); ++normal_count;
		g_sig.connect(&test1, std::placeholders::_1); ++normal_count;
		g_sig.connect(std::bind(&test1, std::placeholders::_1)); ++normal_count;
		//g_sig.connect(&test0); // error
		g_sig.connect(std::bind(&test0)); ++normal_count;
		g_sig.connect(&test2, std::placeholders::_1, 888); ++normal_count;
		g_sig.connect(std::bind(&test2, std::placeholders::_1, 889)); ++normal_count;
		g_sig(101);
		assert(g_call_count == normal_count);
		clear_sigal_state();
	}

	{
		// test raw pointer weak lookup
		ctest* a = new ctest;
		g_sig.connect(&ctest::f1, a, std::placeholders::_1); ++normal_count;
		g_sig.connect(&ctest::f2, a, std::placeholders::_1, cunk()); ++normal_count;
		if (test_warning) {
			g_sig.connect(std::bind(&ctest::f1, a, std::placeholders::_1)); ++normal_count; // [WARNING]lose weak
			warning_count += 1;
		}
		g_sig(201);
		delete a;
		g_sig(202);
		assert(g_call_count == normal_count + warning_count);
		clear_sigal_state();
	}

	{
		// [WARNING] test smart pointer
		if (test_warning) {
			auto a = std::make_shared<ctest>();
			cid = g_sig.connect(&ctest::f1, a, std::placeholders::_1); ++normal_count; // [WARNING]MUST manual disconnect
			warning_count += 1; // same with the 'BOOST'. a was clone to slot container.

			//g_sig.connect(std::bind(&ctest::f1, b, std::placeholders::_1)); ++normal_count; // [WARNING]MUST manual disconnect
			//warning_count += 1; // same with the 'BOOST'. a was clone to slot container.
			
			g_sig(203);
			a.reset();
			g_sig(204);
			if (cid)
				g_sig.disconnect(cid);
			g_sig(205);
		}
		assert(g_call_count == normal_count + warning_count);
		clear_sigal_state();
	}

	{

		// test scope weak
		{
			ctest a;
			{
				ctest b, c;
				g_sig.connect(&ctest::f3_p, &a, std::placeholders::_1, &b); ++normal_count;
				g_sig.connect(&ctest::f3_r, &a, std::placeholders::_1, std::ref(b)); ++normal_count;
#if SIGNALS_ENABLE_REF_BIND
				if (test_warning) {
					g_sig.connect(&ctest::f3_r, &a, std::placeholders::_1, c); ++normal_count; 			// [WARNING] clone with [c]
					warning_count += 1; // [be differ with the 'BOOST', that are += 0]
					g_sig.connect(&ctest::f3_r, &a, std::placeholders::_1, ctest()); ++normal_count; 	// [WARNING] clone with [ctest()]
					warning_count += 1; // [be differ with the 'BOOST', that are += 0]
				}
#endif
				g_sig(301);
	}
			g_sig(302);
}
		g_sig(303);
		assert(g_call_count == normal_count + warning_count);
		clear_sigal_state();


		{
			ctest a;
			g_sig.connect(&ctest::f3_p, &a, std::placeholders::_1, &a); ++normal_count;
#if SIGNALS_ENABLE_REF_BIND
			if (test_warning) {
				g_sig.connect(&ctest::f3_r, &a, std::placeholders::_1, a); ++normal_count; // [WARNING] double clone with [c]
				warning_count += 2; // [be differ with the 'BOOST', that are += 0]
			}
#endif
			g_sig(311);
		}
		g_sig(312);
		assert(g_call_count == normal_count + warning_count);
		clear_sigal_state();
	}

	{
		// test object class scope bind
		{
			ctest_scope a;
			g_sig(401);
		}
		g_sig(402);
		assert(g_call_count == 1);
		clear_sigal_state();
	}

	{
		// [warning] test lambda
		if (test_warning)
		{
			{
				auto lam1 = [](int v){
					printf("lambda=%d\n", v);
					++g_call_count;
				};
				cid = g_sig.connect(lam1); // [MSG] MUST manual disconnect
				g_sig(501);
			}
			g_sig(502);
			g_sig.disconnect(cid);
			g_sig(503);
			warning_count += 2;
		}
		assert(g_call_count == normal_count + warning_count);
		clear_sigal_state();
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	srand((int)time(0));
	todo();

	system("pause");
	return 0;
}

