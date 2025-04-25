一个用于介绍人工智能的玩具，帮助 C++ 程序员更好地理解诊断错误信息。

该玩具实现为一个 clang 插件，因此可以劫持 clang 的诊断消费者以及编译器本身的任何其他信息。

该玩具基于 Clang/LLVM trunk 21.0.git + 15bb1db4a98309f8769fa6d53a52eae62a61fbb2 构建。未测试其他版本。
但鉴于玩具中使用的所有 API 似乎都是稳定的，我猜它也可以与某些旧版本一起使用。

要使用该玩具本身，您需要在 https://bailian.console.aliyun.com/?tab=model#/model-market 上获取阿里云模型服务的 API 密钥。
然后需要将 API 密钥设置在环境变量 "CLANG_AI_KEY" 中。

对于使用其他服务，如果所需的服务支持基于 CURL 的 API，则更新 AIDiagnosticConsumer::HandleDiagnostic（位于
AIDiagnosticConsumer.cpp）应该很容易。

默认模型是 qwen-max。如果您想在上述链接中使用其他模型，可以将环境变量 CLANG_AI_MODEL 设置为您想使用的模型名称。
（有关其他模型的名称，请参阅上述链接）。

默认情况下，AI 将以中文回复。如果您希望 AI 使用其他语言回复，可以将环境变量 CLANG_AI_REPLY_LANG 设置为相应的值。
当要求 AI 使用英语以外的其他语言回复时，AI 会先翻译错误消息，然后再进行解释并提供解决方案。

AI 有一个默认的角色提示。您可以在 AIDiagnosticConsumer.cpp 中看到它。如果您想更改它，可以将环境变量
CLANG_AI_ROLE_PROMPT 设置为您想使用的提示。

利用 AI 诊断编译器中的错误信息具有潜在优势，因为编译器拥有大量代码的内部信息和分析结果。我认为在这方面有很大的改进空间。

如果没有发生错误，插件不会影响编译时间。这一点很重要，因为当前的响应速度并不令人满意，因此将其用于成功的编译任务可能并不合适。


# Example 1

```C++
#include <memory>
class Impl;
class A {
public:
	A();
	
	void whatever();
private:
	std::unique_ptr<Impl> impl;
};

void func() {
	A a;
}
```

Now clang will report:

```
error: invalid application of 'sizeof' to an incomplete type 'Impl'
```

And when I search the error message only, I can't find the information I hope. I can only find it with "unique_ptr".

And with AI, it shows the expected message:

```
$ clang++ test/unique.cpp -fplugin=build/libAIDiagnosticConsumer.so -std=c++17
In file included from test/unique.cpp:1:
In file included from /usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/memory:80:
/usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/bits/unique_ptr.h:79:16: error: invalid application of 'sizeof' to an incomplete type 'Impl'
   79 |         static_assert(sizeof(_Tp)>0,
      |                       ^~~~~~~~~~~
AI Suggestion:
错误信息表明在尝试对不完整类型 `Impl` 使用 `sizeof` 操作符。这通常发生在编译器试图确定类型的大小，但该类型尚未完全定义的时候。

在你的代码中，`Impl` 类仅被声明了 (`class Impl;`) 而没有定义。当 `std::unique_ptr<Impl>` 的析构函数试图执行删除操作时，它需要知道 `Impl` 的完整定义来正确地释放内存。但是因为 `Impl` 是不完整的，编译器无法计算其大小，从而导致了错误。

要解决这个问题，你需要确保 `Impl` 类在使用 `std::unique_ptr<Impl>` 之前已经被完全定义。如果 `Impl` 的定义非常大或者你想保持头文件的整洁，可以将 `Impl` 的定义放在另一个源文件或头文件中，并在需要使用 `std::unique_ptr<Impl>` 的地方包含这个头文件。

例如，如果你有一个 `impl.h` 文件：

    ```cpp
    // impl.h
    class Impl {
        // 定义 Impl 类的内容
    };
    ```

那么在 `unique.cpp` 中，你需要包含这个头文件：

    ```cpp
    #include <memory>
    #include "impl.h"  // 包含 Impl 的定义

    class A {
    public:
        A();

        void whatever();
    private:
        std::unique_ptr<Impl> impl;
    };

    void func() {
        A a;
    }
    ```

这样，`std::unique_ptr<Impl>` 就能够访问到 `Impl` 类的完整定义，而不会产生 `sizeof` 错误。
```

with English:

```
$ CLANG_AI_REPLY_LANG=English clang++ test/unique.cpp -fplugin=build/libAIDiagnosticConsumer.so -std=c++17
In file included from test/unique.cpp:1:
In file included from /usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/memory:80:
/usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/bits/unique_ptr.h:79:16: error: invalid application of 'sizeof' to an incomplete type 'Impl'
   79 |         static_assert(sizeof(_Tp)>0,
      |                       ^~~~~~~~~~~
AI Suggestion:
The error is caused by the use of `std::unique_ptr` with an incomplete type, `Impl`. When you declare a `std::unique_ptr` to a type, that type must be complete at the point of declaration. The `sizeof` operator and the `delete` expression both require a complete type because they need to know the size and proper way to deallocate the memory for the object.

To fix this issue, you have two main options:

1. **Complete the definition of `Impl` before using `std::unique_ptr<Impl>`:**
   Ensure that the full definition of the `Impl` class is available in the translation unit where `A` is defined. This means you should include the header file that contains the `Impl` class definition or define the `Impl` class in the same file before the `A` class.

2. **Forward declare `std::unique_ptr<Impl>` and complete the `Impl` type later:**
   If you cannot or do not want to complete the `Impl` type at the point of declaration, you can forward-declare `std::unique_ptr<Impl>` and then provide a complete type for `Impl` before you instantiate `A` or use any member functions that require `Impl` to be complete.

Here's how you could modify your code if you choose to complete the `Impl` definition:

    ```cpp
    #include <memory>

    // Complete the Impl class definition
    class Impl {
    // ... members and methods of Impl
    };

    class A {
    public:
    A();

    void whatever();
    private:
    std::unique_ptr<Impl> impl;
    };

    void func() {
    A a;
    }

    // Rest of the code...
    ```

If you choose to forward-declare `std::unique_ptr<Impl>` and complete the `Impl` type later, make sure that when you actually create an instance of `A`, the `Impl` class is fully defined.
```

with Japenese (to verify the translation ability): 

```
$ CLANG_AI_REPLY_LANG=Japenese clang++ test/unique.cpp -fplugin=build/libAIDiagnosticConsumer.so -std=c++17
In file included from test/unique.cpp:1:
In file included from /usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/memory:80:
/usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/bits/unique_ptr.h:79:16: error: invalid application of 'sizeof' to an incomplete type 'Impl'
   79 |         static_assert(sizeof(_Tp)>0,
      |                       ^~~~~~~~~~~
AI Suggestion:
エラーメッセージは、テンプレートのインスタンシエーション中に`sizeof`が未完成型`Impl`に対して無効に適用されていることを示しています。これは、コンパイラが`Impl`クラスのサイズを計算しようとしたときにその定義がまだ完全でないためです。

解決策としては、`std::unique_ptr<Impl>`を使用する前に`Impl`クラスの完全な定義が必要です。そのためには、`A`クラス内で`std::unique_ptr<Impl> impl;`を宣言する前に`Impl`クラスを完全に定義するか、または`Impl`の前方宣言と`A`クラスの定義を別ファイルに分割し、`Impl`クラスの完全な定義を含むヘッダファイルを`#include`することで解決できます。

例えば、`Impl`クラスの完全な定義を同一ファイル内に記述する場合：

    ```cpp
    #include <memory>

    class Impl {
    // Implクラスのメンバー変数や関数をここに定義
    };

    class A {
    public:
    A();

    void whatever();
    private:
    std::unique_ptr<Impl> impl;
    };

    void func() {
    A a;
    }
    ```

あるいは、`Impl`クラスを別のヘッダファイルに定義してそれをインクルードする方法もあります。
```

Note that the reply in English is different from the replies in Chinese and Japenese. 
The first line of the reples in Chinese and Japenese will translate the error message first and tell the reasons. 
But the reply in English will tell the reason first.

# Example 2

```C++
#include <vector>
#include <algorithm>
int main()
{
    int a;
    std::vector< std::vector <int> > v;
    std::vector< std::vector <int> >::const_iterator it = std::find( v.begin(), v.end(), a );
}

```

```
$ clang++ -fplugin=build/libAIDiagnosticConsumer.so -std=c++17 test/type-mismatch-in-instantiation.cpp
In file included from test/type-mismatch-in-instantiation.cpp:1:
In file included from /usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/vector:60:
In file included from /usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/bits/stl_algobase.h:71:
/usr/lib/gcc/x86_64-redhat-linux/9/../../../../include/c++/9/bits/predefined_ops.h:241:17: error: invalid operands to binary expression ('std::vector<int>' and 'const int')
  241 |         { return *__it == _M_value; }
      |                  ~~~~~ ^  ~~~~~~~~
AI Suggestion:
错误信息表明在模板实例化过程中，尝试对 `std::vector<int>` 和 `const int` 进行二元运算，这是无效的操作。问题出在 `std::find` 的使用上。

在你的代码中，`v` 是一个 `std::vector<std::vector<int>>`，而 `a` 是一个 `int`。你试图在 `v` 中查找 `a`，但 `v` 中的元素是 `std::vector<int>`，而不是 `int`。因此，类型不匹配导致了错误。

解决方法是确保你在 `std::find` 中使用的值与容器中的元素类型一致。如果你想要在 `v` 中查找包含 `a` 的向量，你需要自定义一个谓词来实现这一点。

以下是修改后的代码示例：

    ```cpp
    #include <vector>
    #include <algorithm>
    #include <iostream>

    bool contains(const std::vector<int>& vec, int value) {
        return std::find(vec.begin(), vec.end(), value) != vec.end();
    }

    int main() {
        int a = 5;
        std::vector<std::vector<int>> v = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

        auto it = std::find_if(v.begin(), v.end(), [a](const std::vector<int>& vec) {
            return contains(vec, a);
        });

        if (it != v.end()) {
            std::cout << "Found vector containing " << a << std::endl;
        } else {
            std::cout << "No vector contains " << a << std::endl;
        }

        return 0;
    }
    ```

在这个示例中，我们定义了一个 `contains` 函数来检查一个 `std::vector<int>` 是否包含某个整数。然后我们使用 `std::find_if` 和一个 lambda 表达式来查找包含 `a` 的向量。
```
