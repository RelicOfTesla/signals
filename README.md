c++11 signals
### Lightweight Signal/slot/trackable

only 92/225/293 source line, lightweight


### sample

```cpp
Signal<void(int)> g_sig;

struct ctest : Trackable
{
	void f1(int v) {
		printf("%s=%d\n", __FUNCTION__, v);
	}

	ctest(){
		g_sig.connect_member(&ctest::f1, this);
		//printf("new\n");
	}
	~ctest(){
		//printf("delete\n");
	}
};
void test1(int v)
{
	printf("%s=%d\n", __FUNCTION__, v);
}
void todo()
{
	g_sig.connect(&test1);
	ctest* a = new ctest;
	g_sig(111);
	{
		ctest b;
		g_sig(222);
	}
	g_sig.connect(&ctest::f1, a, std::placeholders::_1);
	g_sig(333);
	delete a;
	g_sig(444);
}

```

### thanks

* [lsignal](https://github.com/cpp11nullptr/lsignal) 
* [simmesimme](http://simmesimme.github.io/tutorials/2015/09/20/signal-slot) 
