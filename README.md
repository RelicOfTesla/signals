c++11 signals
#Ligh Signal/slot/trackable

only 170 source line


### thanks

* [lsignal](https://github.com/cpp11nullptr/lsignal) 
* [simmesimme](http://simmesimme.github.io/tutorials/2015/09/20/signal-slot) 


### sample

```cpp

Signal<void(int)> g_sig;

struct ctest : Trackable
{
    void f1(int v)
	{
		printf("f1=%d\n", v);
	}

	ctest(){
		g_sig.connect_member(&ctest::f1, this);
		printf("new\n");
	}
	~ctest(){
		printf("delete\n");
	}
};

void todo()
{
	g_sig.connect( &test1 );
	ctest* a = new ctest;
	g_sig(111);
	{
		ctest b;
		g_sig(222);
	}
	g_sig(333);
	delete a;
	g_sig(444);
}

```
