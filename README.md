A toy to introduce AI to help C++ programmers to better understand the diagnostic error messages.

The toy is implemented as a clang plugin then it can be hiject clang's diganostic consumer and any
other information in the compiler itself.

The toy is built with Clang/LLVM trunk 21.0.git + 15bb1db4a98309f8769fa6d53a52eae62a61fbb2. No other
version tested. But given all API used in the toy seems to be stable, I guess it can work with some older
version too.

To use the toy itself, you need to have an API key for aliyun's model service
at https://bailian.console.aliyun.com/?tab=model#/model-market . Then you need to set the API key in the
environment variable "CLANG_AI_KEY".

For using other services, it should be easy
to update `AIDiagnosticConsumer::HandleDiagnostic` in `AIDiagnosticConsumer.cpp` if the service you want support
CURL based API.

The default model is `qwen-max`. If you want to use other model in the above link, you can set the environment
variable `CLANG_AI_MODEL` to the model name you want to use. (See the above link for names of other models).

By default, the AI will reply in Chinese. If you want the AI to reply in other language, you can set the environment
variable `CLANG_AI_REPLY_LANG` to the corresponding value. When the AI is asked to reply in other language than English,
the AI will translate the error message first before explaining and offer the solution.

There was a default role prompt for the AI. You can see it `AIDiagnosticConsumer.cpp`. If you want to change it,
you can set the environment variable `CLANG_AI_ROLE_PROMPT` to the prompt you want to use.

There is a potential advantages of using AI to diagnose error messages in compiler since the compiler have a lot
internal and analyzed information about the code.

The plugin won't affect the compilation time if no error happens. This is important since the current response speed
is not satisfying so it may not be good to use it for successful compilation jobs.

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
