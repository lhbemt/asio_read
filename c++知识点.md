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
## 未整理的知识点-stl
当满足4个条件的时候，讲不会合成默认构造函数，这4个条件是与的关系。其它条件下，将会生成默认构造函数。
1：没有虚函数且没有继承自有虚函数的类。
2：非静态成员在声明时没有进行初始化，即没有用()或者{}的初始化方式初始化类成员。
3：直接继承的基类没有默认构造函数。
4：当所有成员都是class类型或者class类型数组，并且他们的class类型都没有默认构造函数的时候。
一句话总结，只要成员或者继承的父类的成员有默认构造函数，或者父类有构造函数，那么都会生成默认构造函数，否则将不会生成默认构造函数。
第一条很容易理解，当有虚函数的时候，必须设置虚表指针，因此必须生成一个默认构造函数，来设置虚表指针。
第二条也很容易理解，数据成员如果有()或{}初始化，()其实就是=，那么为了初始化成员，也需要生成默认构造函数来做。
第三条也容易理解了，如果父类有默认构造函数，那么子类也必须生成一个默认构造函数，来调用父类的构造函数。
第四条同理。
总结：当一个类对象某一部分需要编译器去做一些事的时候，比如虚表设置，有成员需要成员初始化，有父类构造函数，那么它就会生成默认的构造函数，去干这些事。
一旦程序员主动声明了构造函数，那么编译器在任何条件下，都不会生成默认的构造函数。
如果程序员主动声明了构造函数，那么编译器会给每个构造函数的用户代码前安插调用父类构造函数的代码。如果类的成员是类对象，且有默认构造函数，那么也会给每个构造函数安插成员类的默认构造函数。
注意：默认构造函数指的是()没参数的。比如A::A()就是默认构造函数，而A::A(int i)不是。
这也就是为啥我们类成员有类对象的时候，不需要在构造函数中显示调用类对象构造函数的原因了。

为啥要使用初始化列表的方式初始化类成员呢，因为这样效率是最高的，你直接在构造函数里，写=号赋值，那会出现这样的情况，先创建一个临时对象，然后再调用该类的赋值构造函数，显然，构造函数调用了2次，而直接初始化列表的方式，构造函数只调用了一次。注意，这个是仅对有默认构造函数和赋值构造函数有效，你比如说基础类型，int等，因为没有构造函数，所以初始化列表和在构造函数里=，效率是一样的。此外，初始化顺序是跟你声明时的顺序一致，跟你初始化列表的顺序是无关的。比如你想一个int赋值到2个int成员上，而偷懒使用i(j)，如果i j都是类成员，i在前，j在后，无论你初始化列表顺序怎么写，i都是未定义的，不是你想要赋值的int。

同构造函数一样，当需要编译器去做一些事的时候，就会默认合成拷贝构造函数，这些事包括：类成员有拷贝构造函数(需要正确设置类成员拷贝构造) 父类有拷贝构造函数(需要调用父类的拷贝构造函数) 类有虚函数(需要设置虚表指针)。移动构造函数同拷贝构造函数，移动赋值构造函数也同拷贝构造函数。当然，自己提供了构造函数，肯定编译器不会合成。
所有构造函数的扩充规则，都是为了确保类的完整性，即确保所有成员都是完整的(包括虚函数表指针)

析构函数：同构造函数一样，当我们的类成员和父类有析构函数的时候，我们没有声明析构函数，那么编译器也会为我们合成析构函数，显然，合成的析构函数会调用成员对象的析构函数和父类的析构函数。这个也很容易理解，需要正确的释放类对象。这个和构造函数为了要正确的使用类对象同理。同构造函数一样，当需要编译器做一些事的时候(即类的一部分有析构函数)，它就会合成析构函数。析构顺序是先子类再父类，构造函数则相反，是先父类再子类。
析构函数的扩充规则和构造函数是相反的，构造函数可以理解为在子类构造函数前面插父类或成员的构造函数代码，而析构函数也是在子类析构函数后面插父类或者成员的析构函数代码。类成员对象的析构顺序是和声明顺序相反的，这个和构造函数反着来。
简而言之，编译器会为我们扩充默认的构造函数和析构函数，扩充的构造函数插在代码之前，扩充的析构函数插在代码之后。
通过上面的分析，当有人跟你说，无论一个怎怎样的类，都会合成构造函数和析构函数，显然是错的，狠狠地打脸他，合成是有条件的。这里的构造函数指的是默认构造，拷贝构造，赋值构造，移动构造，移动赋值构造。
构造函数先调用父类的很容易理解，因为你可能在子类中要使用父类的成员，肯定要先初始化，析构也是同理，假如在子类的析构函数中用到了父类的成员，父类如果先被析构了，那肯定子类的析构就出问题了。

函数调用原理：参数压栈压栈是在ebp中，做各种寄存器的操作，然后返回，返回值是在寄存器eax中，所有类成员函数(非静态)在调用的时候，第一个参数总是this指针。相当于A::add(@a, 1, 2)。其中@a是取a的地址，即this指针。这里的类成员包括构造函数，析构函数。所以是可以把类的成员函数看做是全局函数，只是调用的时候，要加个额外参数，this指针。所以，对于非虚函数，它的执行效率和全局函数是一样的，对于虚函数来说，因为它在执行函数时，先要进行一次虚函数表的跳转，才能定位到函数的地址，所以才稍微慢一点。

stl包括六大部件，分别是：1容器 2分配器 3算法 4迭代器 5适配器 6仿函式
容器是存放元素的地方 分配器是如何以内存的方式存储 算法是对元素进行的算法 迭代器是算法和容器的桥梁 而适配器就是转换 仿函数是可以()类。
下面这个例子包含了stl的六大部件：

	#include <vector>
	#include <algorithm>
	#include <functional>
	#include <iostream>
	using namespace std;
	int main() {
		int ia[6] = { 27, 210, 12, 47, 109, 83 };
		vector<int, allocator<int>> vi(ia, ia + 6);
		cout << count_if(vi.begin(), vi.end(), not1(bind2nd(less<int>(), 40)));
		return 0;
	}
其中vector是容器 allocator<int>是分配器 count_if是算法 vi.begin()是迭代器 not1是函数适配器 bind2nd也是函数适配器 less<int>则是仿函数。
适配器是啥意思呢，就是将其装换，比如上面的bind2nd，表示绑定第二个参数，从而形成一个函数，第二个参数是固定的值，这样就转换为只有一个参数的函数了，即调用的是2个参数的函数，但是经过适配转换后，变成了一个参数的函数。这就是适配器。

哈希表的元素大于等于篮子数量的时候，哈希表就要扩充，因为篮子实际上也是用数组做的，这样才能随机访问，所以显然，扩充篮子数也是两倍扩充。原来的篮子也需要重新打散，这就是rehash。所以篮子数量一定是大于等于元素数量的。

随机访问迭代器，什么叫随机访问迭代器，就是迭代器可以加加减减任意位置，也可以乘除。你像数组，是符合的，但是链表是不符合的，因为链表需要next才能找到迭代器的下一个。不能直接通过运算就得到迭代器的位置。
因此，对于容器来说，如果自己带有算法函数，应该使用自带的，效率最高，没有才用全局的算法，一个是效率高，另一个限制是可能不支持，你比如说的sort算法，它需要迭代器的要求是随机访问迭代器，显然，你的list的迭代器就不符合，它不是随机访问迭代器。

类模板无法推导，所以必须显示的尖括号类型，而函数模板可以根据参数推导。所以可以没有尖括号。类模板有泛化和特化。泛化的意思就是定义一个通用模板，而特化的意思，指的是对这个通用的模板实行特定的定义，这样在推导的时候，如果没有特化版本，那就使用泛化版本。比如类型T是泛化，而我写一个int的，就是特化。或者说直接就是某个数，某个特定的东西。特化就是专属版本。特化的语法是前面是template<> class X<int>，比如这个是特化版本的类X，类型是int。那么你需要为这个特化的类重新定义。这个讲的是全特化，还有偏特化。偏特化就是局部特化，比如有两个参数T，对某一部分参数做特化，就叫做偏特化。它的语法就是template<T> class X<int，T>。显然，T就是剩下没特化的参数了。这是第一种偏特化，即模板参数少了，还有另一种偏特化。就是指定类型的特化，比如我原来的参数是T，现在特化为T*，表示如果传入的是一个指针，那么就用这个特化版本的类。比如template<class T> class X，现在特化为template<class T> class X<T*>。可以看到语法是这个样子的，即明确定义了类型是指针，const T*也是一个偏特化版本。

对于分配一块内存来说，由于需要追踪这块内存，所以它会有很多额外的标志，额外的字段，所以分配的内存比你实际的大，你比如说你每次都分配4字节，但是它也许消耗了8字节来存，假如你要100万个4字节那实际分配的内存远远不止400万字节，所以，这就是为什么需要内存池，内存管理，因为每次分配内存的额外字节，大小是固定的，你分配的内存越大，额外字节占用的百分比越小，即浪费越小，反之则浪费越大。
Gnu2.9的编译器，对默认的分配器做了处理，它是这样做的，有一个数组，记录了16个元素，每个元素是某些字节大小链表的指针，每个都是8的倍数，比如0号是要分配8字节大小的结构体，1号是16，依次类推，假如你要分配50字节大小的，那么8的倍数，实际上是由56字节大小的那条链表负责分配内存。它也是一次性要很大一块内存，然后切割成很小一块，用链表串起来，这样就减少了malloc调用次数也就减少了额外字节占用的空间(额外字节可以想象成cookie，用来标识这块内存的)。
但是gnu4.9没有再使用它了，而且默认的std::alloctor，就是没做优化的。改成了pool_alloc，需要自己去显示的调用。内存池一般用freelist实现，即数组，而不是之前我写的链表。

Heap和priority-queue是由vector做数据支撑的。stack和queue是由deque做数据支撑的。

考虑下这个问题，你用指针存数据vs直接用变量存，用指针存看起来对象的大小变小了，但实际它反而占用更多了，因为指针需要额外4字节，而开辟的那块内存，也是一样的大小，所以当只有有好处的时候，你才应该这么干，比如很多情况的move，那显然指针好。其它情况好像没啥必要，因为引用传值，也能代替这个指针。

Stl的双向链表是环状的，它多加了一个空白节点，用来符合stl的左闭右开，即空白节点的尾指向链表头，而链表的尾指向空白节点的头。iterator迭代器就是从链表头开始，找到最后就是空白节点，表示end。

迭代器是一个类，在容器中用type_def的方式，变成iterator，然后在使用的时候，每次::iterator，都是创建一个新的类对象，并将自己作为构造成员传进去，
operator++()是++i版本，而operator++(int)则是i++版本。

Iterator类一定要有5个typedef，traits，traits是特征特性，特值的意思。就是你自己设计的迭代器，一定要有这5个typedef，才符合迭代器的traits，比如type，就有type traits。这个traits即你的类符合traits要求的东西，那你就是这个traits，你传进去的类对象，就能通过这个traits得到相应的特征，有点像python的鸭子模型。如：iterator_traits<iter>::iterator_category，即获取这种迭代器的分类，即属于哪种类型。算法会根据迭代器的分类，来采取最佳的算法，比如支持+n的，那就不需要一步一步走来模拟+n，value_type就是所指向元素的类型。difference-type就是迭代器相减是什么类型。另外两个类型是reference_type和pointer_type。iterator_traits就是根据传入的T，转换出那5种traits，当传入的T不是类，那就用模板特化的特性，转换出那5种traits，牛逼啊，这样就统一了。所以想获取type的时候，直接用iterator_traits一包就完事了，你自己的也能模仿，只要实现特化就行，太牛逼了。traits的设计有点像适配器。

当vector扩充2倍时，无法找到这么一块2倍内存的时候，扩充就失败了，此时就无法再放入元素了。vector本身只有3个成员，指向数组起始地址的iterator begin，指向终点的end，指向capacity的指针。所以是有3个iterator，当我们push的时候，直接就是end指针的移动，那你会想，它的数组哪里去了，实际上是由allocate管理的，begin实际上是指向allocate返回的指针，牛逼啊，这样就做出了分离。那块内存由allocator管理。copy的时候，你会想，直接memcopy不就行了？显然不行，为啥呢，因为你原来的内存要被释放，那原来的就会调用析构函数，那假如你成员有指针，那就会变成空指针了，那显然会炸，所以要调用拷贝构造函数，重新构造。显然，要try catch这段代码，当拷贝构造中出现问题时，释放这段内存。

deque的实现很有意思，它是逻辑层上的连续内存空间，实际上是由多个指针形成的指针数组，每个指针指向一块内存，所以才可以双向进出。双向存取。分段连续内存，再组合成逻辑上的连续内存。迭代器的node指向这块内存数组，++迭代器显然就可以走完某个指针指向的内存，然后再指向下一块内存指针。看起来所有容器，都是一个迭代器指向头，一个迭代器指向尾，然后其它成员则是指向allocator分配的内存指针。deque因为是双向开端的，所以它在某个位置插入元素的时候，它会去判断离头更近，还是离尾更近，从而移动更少的元素，来完成插入操作，我们都知道，移动元素会有构造和析构。

当你的操作符重载支持前++和后++，那么后++的实现一定是调用前++，很容易理解，因为你不需要写两遍。。

queue和stack都是用deque实现的。

无锁队列是通过原子操作来代替mutex这种互斥量的，并不是真正的无锁。。cas操作就是所谓的compare exchange操作，底层是操作系统的原子操作。用的是compare and swap操作，判断当前尾部是否指向NULL，是则交换设置node。

物理核心就是cpu的数量，比如8核，逻辑核心就是每个核可以进行的线程数量，比如每个物理核心2线程，那么总共就是16个逻辑核心。即，可以同时运行16个线程。原子操作在cpu中是不可再分的，而非原子操作，在cpu中会分成多条指令来执行，这就是为什么i++，在两个线程运行时，如果不加锁会出问题的原因，因为i++这一个语句，是多条指令。例如是：load i i++ store i，正常是这三步，假如2个线程执行它，那如果是正常的这3条语句顺序，线程1执行完，再线程2执行。那么没啥问题，i会是2，但是，假如线程1是load i 线程2也是load i，那最后，i的值只会是1了。。因为2个load i的时间点的时候i是1。cas操作就是来保证这个操作的，假如两个线程同时用compare exchange告诉cpu，我要把0变成1，那么显然只有一个会成功，因为另一个来做这个操作的时候，大小已经是1了，不等于0。所以操作失败。所以失败的那个线程应该用while循环，来一直重试比较交换操作，这就是自旋锁。

分布式锁，就是多台机器，要访问一个竞争资源的时候，就需要锁，这个锁非常简单。。可以是一个简单的变量，比如0表示没上锁，1表示上锁，而在上锁和解锁，都是通过通信来完成的。。。可见，分布式锁比单机锁，简单非常非常多。当然，有一定的时效性，不然你就一直锁着了，这个可以用redis的expire字段。。太简单了。。

Lockfree是因为你可以在返回false的时候，去干其它事，然后等会再来获取锁，而mutex，你会一直被阻塞住。。这就是lockfree和mutex的区别。lockfree不一定是最好的，它只是不会阻塞你的线程而已。当你要一直获取的时候，也是在自旋的。而mutex是阻塞。所以要不要用lockfree，看你的应用场景，是不是一定不能被阻塞。还有就是你可以压测，看一下差别。

临界区越小越好，一定要注意这一点，用std::lock_guard的时候，一定要临界区越小越好。用{}包起来就好，这样临界区结束，立马就释放锁了。std::atomic是一个模板类，T可以是任意的结构体。compare_exchange_strong是用在once，即只对比一次的，而compare_exchange_weak是用在loop的时候，即要一直等待，所以自旋锁，应该用这个weak。因为weak性能更好。虽然atomic可以用赋值=的方式去存储或者获取原子值，但是，不要这样做，这样就体现不出你是原子变量，应该用store和load。虽然=号被重载了。记住，atomic的使用，一定要用它的api，比如load store exchange compare_exchange_strong。因为如果你用普通的++，中间发生了什么事，你都不知道，可能你刚好让出了cpu，你回来的时候，已经是另一个值了，所以你应该使用exchange这个函数。同理，+=那些操作符，你应该使用fetch_开头的函数。而不是直接用+=。atomic有个函数，叫is_lock_free，来判断，它是不是lock_free的，记住，lock_free表示不是阻塞，而非lock_free是会阻塞的，lock_free一定是原子的，而原子的不一定是lock_free的。基本上memory_order就不要去考虑了，atomic足够了。。float也可以是atomic的，但是，它不能用compare，确实，浮点数比较没啥意义。如果是自定义的类，那么，都应该定义成类指针的atomic，这样才是lock_free的，注意，lock_free是非阻塞。比如你在gui线程和另一个线程交换数据，显然，你不能在gui线程阻塞，这样会卡住，所以必须上lock-free。load要先load出来，保存在一个变量里，然后再使用。

还有个很细的细节，就是allocate和deallocate也是会阻塞的，即分配内存和释放内存，也是会阻塞的，所以在实时线程中，也要注意。

所以，无锁队列的3大要求：1无数据竞争 2无allocate和deallocate 3lock-free。第二点有点难哦，意味着我们要事先分配固定大小的空间，并且使用ring-buffer实现。ring-buffer是循环数组的意思。第二点是不是可以放松下，不然你也不知道事先该让这个队列多大呀。否则的话，满了就push失败了，返回false，这样也行吧，同理pop失败也是返回false，这样也确实不会阻塞。

constexpr的使用，这个是编译的时候，可以计算出来的值，你你比如说template<typename T，size_t size>显然，这个size是在编译期间就知道的值，那么我们就可以这样用constexpr ringBufferSize = size + 1。卧槽，太细节了。

写线程安全的代码，一定要代入两个线程，去想会不会出现问题，比如第二个线程的值把第一个线程的值覆盖了。

红黑树是一种平衡二叉搜索树。红黑树的每个节点都是红色或者黑色，代表节点的某种状态。所以在插入数据的时候，树会自动的平衡，最后在插完数据后，它是一颗已经排列好了的树。所以，map的begin指向的就是最左边的子节点，end指向的是最右边的子节点，所以遍历的时候，一定是按key从小到大排序的，你可以试验一下。确实如此。迭代器++的时候，就是从最左边的子节点开始往中间节点，再往右遍历。对map来说，key不可以修改，因为会破坏红黑树，而value当然是可以改的。注意，你可以改变map的key的比较方式，只需要传一个cmp函数进去就可以了。它会按照你的cmp函数来排序key。注意，当你的类是没有任何成员的，但是，它又是另一个类的成员，前面说过空类的大小是1，再加上还要对齐，所以计算大小，别忘了这个。普通list一样，为了适应左闭右开的规则，map也有一个空节点，叫做header。因为我们map底层用的是红黑树，所以实际上，我们在程序中，也可以直接用rb_tree，而不用去找第三方的库，hash_table同理。insert_unique是唯一的key，insert_equal是允许插入相同的key。gnu4.9也实现了handle and body的面向对象思想，和asio一样，即handle和body分离，body是真正实现的一个类，而handle是这个类的实例化指针或者对象，这样body可以随便你实现，只要有handle提供的函数就可以了，比如asio里面的handle有poll run函数，而body可以是select epoll poll iocp等。所以使用算法find，传入的是两个迭代器，那它肯定按迭代器的加法去运做，从左中右的方式遍历。所以速度慢很多。而自带的find，是从head开始，直接二分查找了。map的valuetype是一个std::pair它自己根据key和value包装成了pair，为了不让修改key，把pair的第一个参数，改成了const key。它的keyofvalue是select1st。map的[]很坑。当[]里面的key不存在的时候，它会自动创建，并且，operator[]会先查找key的lowerbound，然后再insert，所以它显然比直接insert更慢，所以不要用习惯了python，就直接用[]来赋值。。

哈希表。前面说过，当元素数量大于等于bucket大小的时候，需要rehash，rehash就是扩充桶的数量，即分配一块新的内存数组，存放所有的bucket。为啥需要bucket的数量大于等于元素个数呢，因为这样每个bucket上面的list会足够小。rehash的原因是避免某个bucket上的链表过长，什么时候过长呢，这里完全没数学公式什么的，完全是人的经验法则，这个经验法则就是，当元素数量大于等于bucket的数量时，就需要rehash。那么rehash的过程是什么呢：首先把bucket的数量扩充为原来的2倍然后选附近的质数。所有的元素重新计算hash函数，从而落到不同的篮子里，因此，我们一开始就需要把篮子的数量先定义个差不多能用的，这样才能减少rehash的次数，因为你rehash需要分配内存，需要释放原内存，每个元素还需要重新计算hash函数值。所以这个hash函数要跟篮子数量有关，不然假如与篮子数量无关，你rehash一下，还是回到了原来的篮子。还有个经验就是，一开始篮子的数量，大家都会以质数的方式设置，gnu的篮子数量，初始值就是53。假如要扩展。那么就会选106附近的质数。选到了97。所以53个bucket就扩充为97个bucket，然后重新计算hash函数。所以一开始会选定所有从53开始的算好的2倍附近的质数。放在staic数组里。最终有个大小，这样才不会在运行时去计算需要扩充到哪个大小。所以rehash是很花时间的。注意，它里面的hash函数是拿到编号来计算的，那个编号怎么计算，可以由外面给定。注意这一点。相当于有2个hash函数，内部的hash函数是根据编号来计算放在哪个篮子里，而编号怎么计算，可以由你传入的hash函数来定。注意这2个hash函数。内部的hash函数其实非常简单，就是根据编号，计算余数，就是放在哪个bucket里。而外部的hash函数，就可以你来定，反正算出来的编号，都是再hash，放到某个篮子里。算出来的编号，就是hashcode。hashcode除以bucket大小的余数，就是bucket的位置。

迭代器的分类：input_iterator output_iterator forward_iterator bidirectional_iterator random_access_iterator
除output_iterator外，其余4个是继承关系，其中forward_iterator继承自input_iterator bidirectional_iterator继承自forward_iterator，random_access_iterator继承自bidirectional_iterator。
random_access表示可以随机访问，即可以+n -n，比如array vector dequeue。bidirectional表示可以两边++和--，所以list是这个，map multimap，set multiset都是。forward显然只能前进，所以forwardlist是这个。而如果hashtable的链表是双向的那么unordered系列就是bidirectional的，否则是forward的，unorder系列基本上都是forward的。istream_iterator表示input iterator，osream_iterator是output iterator。input iterator表示可以从这个迭代器里取元素，而output iterator表示可以把元素写到这个迭代器里。注意，input iterator的迭代器原来必须存在，所以才可以存取，而output iterator原来的迭代器可以不存在，而且可以移动的时候写进去。你像input如果原来不存在，你移动的话就会走到end，而output原来不存在，你也可以移动下去，直到指定的end。

算法需要知道迭代器的分类，执行最佳的算法，比如random就可以随便+n，而你如果是forward，就需要走n次循环。比如distance这个算法。注意，函数模板是没有特化的，它只有重载。copy算法就是对函数模板的一种解释，它会根据传入的不同T，来匹配不同的函数，从而进行是memcpy还是调用赋值构造函数。当没有有用的构造函数的时候，使用memcpy，否则使用赋值构造函数，当T是char*的时候，调用memcpy。显然，只有深拷贝，需要写拷贝构造，赋值构造。那么他们的拷贝构造和赋值构造就是non-traival的，否则不需要手动写的拷贝构造，赋值构造就是triaval的，这是一个优化，通过type_traits，就可以得到是non-traival还是travial的，显然non-traival要调用自身的构造函数，而traival只需要调用memcpy就行，显然，memcpy快非常多。

算法提供了一个binary search，但它要求迭代器先是已经排序了的，所以，对于顺序性容器，如果要快速查找，指的是非常大的数据的情况下，应该先使用sort先排序(一般内部实现是快排)，然后再使用二分查找。lowerbound，指的是在排序后的元素中，能插入的地方的最低点，upperbound就是最高点。比如10 10 20 20 20 30，要插入20，那么lowerbound就是10后面的20，upperbound就是30前面的20。是找到这个安插点，你可以进行安插，也可以判断安插点是不是某个值从而判断是否存在。

仿函数的可适配条件，需要继承std::binary_function和std::unary_function。binary_function的意思就是两个操作数一个返回值，而unary_function则是一个操作数一个返回值。stl规定，每一个仿函数，都应该挑一个适配器来继承。就是上面的unary或者binary。继承了这两个，就获得了几个typedef，分别是第一参数类型，第二参数类型，返回值类型。有了这些typedef，在bind1st bind2nd的时候，就可以适配了，即为了函数适配器，需要这些typedef，这个适配器也类似于traits，问你有什么东西。你就回答出去。所以，如果你希望你的仿函数能够被bind1st和bind2nd，你就必须能回答那3个问题，那么要么你自己typedef，要么就继承unary或binary，显然，你愿意继承而不是自己写。

适配器是啥，就是改造器，比如3个参数的函数改造成2个，2个参数的改造成一个。
容器适配器：stack和queue，它是由deque改造而来的。它底层就是有一个deque容器，所有成员函数都交给deque来做。改一下名字即可。。
函数适配器：就是前面说的binder2nd等等适配器。binder2nd表示允许你把第二个参数绑定为一个特定的值。它实际上是记录第二参数和你的函数，然后重载operator()，在调用的时候，将第一参数和记录的第二参数作为记录的函数的参数，执行并返回。c++11后，bin2nd和bind1st已经过时了，现在统一是bind函数，它其实是创建一个bind类对象，和也是一个适配器，同样的实现。

Not1是非的意思，它是取否定的适配器。

std::bind可以绑定函数，可以绑定仿函数，可以绑定成员函数(第一个参数必须是类对象地址)，可以绑定数据成员(这个也能绑定啊，确实没用过。第一个参数也必须是对象的地址)。绑定数据成员的时候返回值是直接取数据成员的值，而绑定其它3个的时候，返回值是函数的执行返回值。占位符_1 _2 _3等等表示之后传入参数，而不是现在传入。现在传入的是确定值。有占位符后，你调用这个bind后的对象，在调用时就需要传占位符的参数，否则不需要。bind<T>表示改变返回值的类型为T，我靠，这也太牛了，连返回值类型也能重新绑定。。绑定成员函数或者数据成员，_1必须存在，因为它是this指针，在调用的时候，必须把对象地址传进去。

某些迭代器也是适配器改造的。revers_iterator就是。它底层是有另一种迭代器，所有操作是由另一种迭代器来完成。所以适配器模式的意思就是，一个新的东西，是由旧的东西改造而来，新的东西的操作是由旧的来完成。inserter_iterator也是一种迭代器适配器。为啥需要这个迭代器呢，是因为你的copy函数，如果目标迭代器的空间不足的话，copy就炸了，所以需要确保copy的目标空间足够，这就是inserter_iterator。inserter_itetstor当目标空间不足时，它会开辟新的空间，insert_iterator包装后，会返回一个迭代器，它能保证这个迭代器的空间是存在的。并且它是不会覆盖原来的，而且在某个迭代器开始插入多少个元素，注意是插入，原来的会往后退。所以inserter_iterator是一种插入迭代器，往传入的某个迭代器位置开始插入n个元素，不够就开辟新空间。它底层是重写了operator=，调用容器的insert函数，插入位置是当前迭代器。然后迭代器++。

ostream_iterator是一种x适配器，x适配器的意思就是未知的意思。它既不是容器适配器，也不是迭代器适配器，也不是函数适配器。它是一种迭代器，所以可以作用于算法，它需要绑定到一个ostream对象。所以往这个迭代器放入就是往ostream对象写入元素。第二参数是一个字符串，作为分隔符。它也是重载了操作符operator=，在里面进行ostream的operstor<<操作。重写operator++和--里面是return*this，表示啥都不操作。因为它修饰的是ostream，而ostream可以是cout，可以是文件等等，所以它是x适配器。不好分类成哪一种适配器。显然istream_iterator修饰的就是istream了。istream_iterator的用法比较奇怪，它要先定义一个无任何参数的对象，来表示结束的iterstor，再定义一个有参数，即有绑定istream的iterator来作为开始的iterstor，这样开始操作迭代器。需要这个结束迭代器的原因是，istream执行的是operator>>操作，当操作失败，表示无数据写入的时候，istream就是无效状态，而这个无效状态，就是结束的iterator，所以需要做比对，才知道结束了。

一个万用的hash function，我们都知道hash function是用来算hash code的。c++11提供了一个万能的哈希函数，hash_val。用这个就行了，传入所有的基础数据成员。

Tuple是元组，pair因为是二元的，所以不够的时候，你可以用tuple，tuple的意义是可以由多个不同类型组成，但是结构体也能做到，而且意义更明显，所以tuple感觉没啥意义，你tuple是0 1 2 3这样取值，很糟糕的阅读。。不要用它。

typetraits很有用。它是一个模板，模板里面有几个typedef，用于标志一些东西，分别是has_traival_default_construct has_traival_default_copy_construct等等那几种构造函数和析构函数，traival的意思就是平凡的，不重要的，前面copy的时候看到过，如果是traival_copy_construct，那么它直接用的是memcpy，速度非常快，否则是调用拷贝构造函数。显然，只有深拷贝是需要的。还有个是is_pod_type。默认都是false，表示都重要。所以有特化版本，你比如int就是true，表示都不重要，所以你也可以为你的类写一个偏特化版本，告诉这些重不重要，不重要的话，在copy的时候，将执行的很快。podtype看起来是基础类型的意思，比如int double等等。可以说，只有带着指针的这一种。拷贝和析构函数才是重要的。或者是关于资源控制的，比如关闭文件等等，也是重要的。这个typetraits是为算法服务的，比如copy算法。c++11后，提供了非常多的type_traits，都是isxxx的typedef。pod是平淡如水，平常的数据类型。旧的格式，就是c里面的数据格式。即，里面只有data没有function的类，就是pod。c++11后，它不需要你自己去为你的class写这些偏特化。编译器自己知道。is开头的那些就是typetraits，在c++11的版本里。

写一个程序，测试下插入100万个元素时，map和unordermap的插入速度和最后所占的空间对比。


