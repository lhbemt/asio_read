# 看asio里面例子中的c++知识点
## std::size_t
std::size_t是一个无符号整数类型，可以存放下理论上可能存在的对象的最大大小。一般用于数组索引和循环计数，比如分配内存时等。
## std::aligned_storage
分配一块未初始化的内存，且是对齐的，对齐方式是alignof(T)，就是每次内存地址+1，实际地址是+aligof(T)的。但是还是需要自己使用operator new去分配内存。例子：这个是使用内存池的一种创建方式使用对齐 加速内存访问

	template<class T, std::size_t N>
	class static_vector {
		// N个T的正确对齐的未初始化存储
		typename std::aligned_storage<sizeof(T), alignof(T)>::type data[N];
		std::size_t m_size = 0;
		public:
			template<typename ...Args> void emplace_back(Args&&... args) {
				if(m_size >= N) {
					throw std::bad_alloc{};
				}
				new(data+m_size) T(std::forward<Args>(args)...); // 这里的new后面加参数 表示在指定的地址上分配内存，所以要使用对齐的方式 使用内存
				++m_size;
			}
			// 访问对齐存储中的对象 因为使用了对齐的方式 所以很轻易的就根据对齐字节获得要访问的地址
			const T& operator[](std::size_t pos) const {
				return *reinterpret_cast<const T*>(data+pos);
			}
			// 从对齐存储删除对象
			~static_vector() {
				for(std::size_t pos = 0; pos < m_size; ++pos) {
					reinterpret_cast<T*>(data+pos)->~T();
				}
			}
	}
删除则需要主动调析构函数。
## reinterpret_cast无视任何限制的转换
const_cast是添加或移除const static_cast是进行基本转换 dynamic_cast进行有检查的多态转换(基类转派生类) reinterpret_cast是进行各种类型的转换，但是不能去除const，有关const的转换，需要使用const_cast。
static_cast完成相关类型之间的转换，而reinterpret_cast可以在任何两个类型间进行转换anything即用于处理互不相关的类型。
static_cast会进行截断，比如从float转为int，而reinterpret_cast完全就是memcpy形式的，不会做任何截断，不会丢失比特位。
dynamic_cast使用到多态中更好的原因是它会进行运行期检查，即检查是不是继承关系的类(是否有virtual函数)，从父类转子类在运行期间，会炸，而子类转父类不会。而static_cast则不会进行检查，reinterpret_cast也不会检查。
换言之dynamic_cast最严格，static_cast和reinterpret_cast都不严格。当继承类没有virtual函数的时候，只能用static_cast或者reinterpret_cast进行转换了。
reinterpret_cast非常强大，完全无视了各种限制。。static_cast会在编译期就进行检查，当是私有继承的时候，static_cast直接就编译失败了，如下：

	class A {
	public:
		virtual void print() {
			std::cout << "print A" << std::endl;
		}
	};
	class B : public A {
	public:
		int m;
		int n;
	public:
		void print() {
			std::cout << "print B" << std::endl;
		}
	};
	int main(int argc, char* argv[]) {
		B b;
		A a;
		A* ap = reinterpret_cast<A*>(&b); // 可以转换 即使是不是public继承，运行期间仍然正确运行。完全无视各种限制
		ap->print();
		A* ap_p = dynamic_cast<A*>(&b); // 当B继承A无public时(即private继承)，可以转换，但是在运行期间直接炸了。
		ap_p->print();
		A* ap_pp = static_cast<A*>(&b); // 当B继承A无public时(即private继承)，在编译期间就直接炸了
		ap_pp->print();
		return 0;
	}
用一句话总结，reinterpret_cast非常自由，完全无视任何编译运行规则(比如上面的私有继承)，dynamic_cast在运行期间会检测，static_cast在编译期间会检测。
所以在编写代码中，还是少用reinterpret_cast，它太自由了，可以突破一切限制。。要const转换用const_cast，要多态转换用dynamic_cast，其余都用static_cast。除非你知道你在干什么，才用reinterpret_cast。
## noexcept
noexcept是告知该函数将不会抛出异常，noexcept()表达式则是检测表达式是否会抛出异常，不会返回false，会返回true。
## std::error_code
std::error_code是一个依赖平台的错误码，比如一些常见的系统错误error_code，如文件不存在等error_code，现代c++要用这个。
## std::exception
std::exception是异常类的基类，通过继承它并重写const charr* what()函数，可以在抛出异常时捕获自己的异常，并得知是何异常引起的。
## std::array
std::array是创建一个固定大小的数组，和c语言里面的数组是一样的。
## std::enable_shared_from_this<T>
类继承自std::enable_shared_from_this<T>，那么这个类就拥有了shared_from_this()这个函数，这个函数是干嘛用的呢，这个函数是可以让一个已经被shared_ptr管理的对象，安全的生成一个新的shared_ptr对象，即引用计数+1了。就是说是共享了这个对象，而你使用shard_prt(this)指针，构建的是另一个shared_ptr，并不是共享同一个对象的，这将会导致this被释放两次，从而导致程序崩溃，这个可以通过use_count来计算出。很明显，当你的类的shared_ptr要被多个线程，或者是异步使用的时候，为了保证在回调的时候，改变的是同一个对象且这对象仍然存在，需要使用这个特性。c++17支持的weak_from_this显然就是返回一个弱引用了。
一句话shared_from_this()使其shared_ptr引用计数+1。即延长其对象的生命周期(不能通过由指针创建新的shared_ptr，这样会导致同一个指针被释放多次，引起崩溃)。	

	class A : public std::enable_shared_from_this<A> {
	public:
		std::shared_ptr<A> get_ptr() {
			return shared_from_this();
		}
		std::shared_ptr<A> get_ptr_not_same() {
			return std::shared_ptr<A>(this); // 生成的新的shared_ptr对象 导致被析构两次 程序崩溃
		}
	};
	void use_ptr(std::shared_ptr<A> a) {
		std::shared_ptr<A> b = a;
		std::cout << "use_cout: " << a.use_count() << std::endl; // use_count是4 离开作用域会减去2
	}
	int main(int argc, char* argv[]) {
		std::shared_ptr<A> ap(new A);
		std::cout << "use_count: " << ap.use_count() << std::endl; // use_count是1
		auto ap1 = ap->get_ptr();
		std::cout << "use_count: " << ap1.use_count() << std::endl; // use_count是2
		use_ptr(ap);
		std::cout << "use_count: " << ap1.use_count() << std::endl; // use_count是2
		auto ap2 = ap->get_ptr_not_same();
		std::cout << "use_count: " << ap2.use_count() << std::endl; // use_count是1 之后释放时程序崩溃 因为this指针被放到了两个不同的shared_ptr 最后this被释放了2次
		std::cin.get();
		return 0;
	}
## constexpr
constexpr是在编译期间就可以求得函数或者变量的值，可以减少在运行期间的运算。constexpr变量的要求：1其类型必须是字面类型(标量类型 引用类型 字面类型的数组 void) 2它必须被立即初始化 3其初始化的隐式转换，构造函数等都必须是常量表达式。
constexpr函数的要求：1其返回值必须是字面类型 2其每个参数都必须是字面类型 3至少存在一组实参，是的函数的每一个调用为核心常量表达式的被求值的子表达式
	
	constexpr int day_seconds() {
		return 24*60*60
	}
	constexpr int factorial(int n) {
		return n <= 1 ? 1 : (n * factorial(n - 1));
	}
因为这个constexpr能在编译期间计算的特性，很多都能加速程序的运行，比如上面的求阶层，只要编译时，能确定n的值，那么就可以在编译期间计算，int n = factorial(5);从而加速访问，也许很多数学表达式可以用这个constexpr特性来做。这也是一个优化的方向。当然如果你传入的n是一个变量，这次计算结果还是要通过函数来求，因为无法在编译期间知道n的值，它会退化成函数，自动忽略constexpr。
## 构造函数中抛出异常
构造函数中可以抛出异常，但是抛出异常时，析构函数是不会被调用的，所以要保证在抛出异常时，资源已经被清理了，否则就会造成系统资源的泄露。
## std::map问题
因为写python写习惯了，在python中，直接[]会报错，但是在c++中，是不会的，它会给你一个默认值，比如你的value是std::shared_ptr，就会导致key不存在时，value是一个nullptr。巨坑。。。得注意c++的map和python中不一样的，c++中，直接[]取值会给你默认值。。


