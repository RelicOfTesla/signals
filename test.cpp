#include "stdafx.h"
#include <cassert>
#include <cstdio>
#include <ctime>
#include <tchar.h>

//#define TEST_CLONE_BIND 1
//#define SIGNAL_OPEN_CLONE_BIND 1

#include "signal.hpp"

Signal<void(int)> g_sig;

int g_call_count = 0;

struct cunk{
	void f1(int v){
		printf("%s=%d\n", __FUNCTION__, v);
		++g_call_count;
	}
};
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
		printf("%s=%d\n", __FUNCTION__, v);
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
	int WARNING_Ref = 0;
	SignalConnect conn;

	auto clear_sigal_state = [&](){
		g_call_count = 0;
		WARNING_Ref = 0;
		normal_count = 0;
		conn.reset();
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
		ctest* a1 = new ctest;
		g_sig.connect(&ctest::f1, a1, std::placeholders::_1); ++normal_count;
		g_sig.connect(&ctest::f2, a1, std::placeholders::_1, cunk()); ++normal_count;
		if (test_warning) {
			// [WARNING]not support visit std::bind raw pointer
			g_sig.connect(std::bind(&ctest::f1, a1, std::placeholders::_1)); ++normal_count;
			WARNING_Ref += 1;
		}
		g_sig(201);
		delete a1;
		g_sig(202);
		assert(g_call_count == normal_count + WARNING_Ref);
		clear_sigal_state();
	}
	

	{
		// test smart pointer
		// [MSG] Odd Style
		{
			auto a1 = std::make_shared<ctest>();
			g_sig.connect(&ctest::f1, a1.get(), std::placeholders::_1); ++normal_count;
			g_sig(221);
		}
		g_sig(222);

		if (test_warning) 
		{
			SignalConnect conn3;
			{
				// [WARNING]MUST manual disconnect
				auto b1 = std::make_shared<ctest>();
				conn = g_sig.connect(&ctest::f1, b1, std::placeholders::_1); ++normal_count;
				WARNING_Ref += 1; // same with the 'BOOST'. [b1] was wrap/clone in slot container.

				// [WARNING]MUST manual disconnect
				auto c1 = std::make_shared<ctest>();
				conn3 = g_sig.connect(std::bind(&ctest::f1, c1, std::placeholders::_1)); ++normal_count;
				WARNING_Ref += 1; // same with the 'BOOST'. [c1] was wrap/clone in bind.

				g_sig(223);
			}
			g_sig(224);

			if (conn)
				conn->disconnect();
			if (conn3)
				conn3->disconnect();
			g_sig(225);
		}
		assert(g_call_count == normal_count + WARNING_Ref);
		clear_sigal_state();
	}

	{
		// test scope weak
		{
			ctest b1,c1;
			g_sig.connect(&ctest::f1, &b1, std::placeholders::_1); ++normal_count;
			g_sig.connect(&ctest::f1, std::ref(b1), std::placeholders::_1); ++normal_count;
#if TEST_CLONE_BIND
			if (test_warning) {
				g_sig.connect(&ctest::f1, c1, std::placeholders::_1); ++normal_count;
				WARNING_Ref += 1;
			}
#endif
			g_sig(203);
		}
		g_sig(204);

		assert(g_call_count == normal_count + WARNING_Ref);
		clear_sigal_state();
	}
	{
		// test scope weak2
		{
			ctest a1;
			{
				ctest b1, c1;
				g_sig.connect(&ctest::f3_p, &a1, std::placeholders::_1, &b1); ++normal_count;
				g_sig.connect(&ctest::f3_r, &a1, std::placeholders::_1, std::ref(b1)); ++normal_count;
#if TEST_CLONE_BIND
				if (test_warning) {
					// [WARNING] clone with [c1]
					g_sig.connect(&ctest::f3_r, &a1, std::placeholders::_1, c1); ++normal_count;
					WARNING_Ref += 1;

					// [WARNING] clone with [ctest()]
					g_sig.connect(&ctest::f3_r, &a1, std::placeholders::_1, ctest()); ++normal_count;
					WARNING_Ref += 1;
				}
#endif
				g_sig(301);
			}
			g_sig(302);
		}
		g_sig(303);
		assert(g_call_count == normal_count + WARNING_Ref);
		clear_sigal_state();


		{
			ctest a1;
			g_sig.connect(&ctest::f3_p, &a1, std::placeholders::_1, &a1); ++normal_count;
#if TEST_CLONE_BIND
			if (test_warning) {
				// [WARNING] double clone with [a1]
				g_sig.connect(&ctest::f3_r, &a1, std::placeholders::_1, a1); ++normal_count;
				WARNING_Ref += 2;
			}
#endif
			g_sig(311);
		}
		g_sig(312);
		assert(g_call_count == normal_count + WARNING_Ref);
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
				conn = g_sig.connect(lam1); // [MSG] MUST manual disconnect
				g_sig(501);
			}
			g_sig(502);
			conn->disconnect();
			g_sig(503);
			WARNING_Ref += 2;
		}
		assert(g_call_count == normal_count + WARNING_Ref);
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

