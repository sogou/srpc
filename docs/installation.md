# 安装

## 1. Linux源码安装

### 1.1 前置依赖

源码安装需要一些前置依赖：
* 手动安装：CMake（要求3.6以上）、OpenSSL(推荐1.1及以上)、Protobuf（要求3.5及以上）
* 自动安装：Workflow、压缩库snappy和lz4  

Workflow、snappy和lz4也可以系统预装，如果third_party中没有通过`--recursive`参数拉取源码依赖，则会从系统默认安装路径寻找，snappy的预装要求版本是v1.1.6或以上。

其中前置依赖`Protobuf`，源码安装参考：

~~~sh
git clone -b 3.20.x https://github.com/protocolbuffers/protobuf.git protobuf.3.20
cd protobuf.3.20
sh autogen.sh
./configure
make -j4
make install
~~~

### 1.2 源码安装SRPC

支持make命令、bazel编译或者cmake命令编译，默认会编译出：
- 静态库：libsrpc.a（或者dylib）
- 动态库：libsrpc.so（或者dll）
- 用于生成代码的二进制工具：srpc_generator

以下命令三选一即可：

- **make**
   ~~~sh
   git clone --recursive https://github.com/sogou/srpc.git
   cd srpc
   make
   ~~~

   执行以下命令安装到系统默认路径（可选，如果不执行，则自己开发代码时需要注意指定寻找SRPC库和头文件的路径）：
   ~~~sh
   make install
   ~~~

- **bazel**
   ~~~sh
   bazel build ...
   #可以编译出lib和srpc_generator以及所有示例编译出来的可执行文件，在bazel-bin/目录下
   ~~~

- **cmake**
   ~~~sh
   mkdir build.cmake
   cd build.cmake
   cmake ..
   make # 可以编译出lib和srpc_generator
   ~~~

### 1.3 编译教程示例

tutorial目录下会默认编译出所有示例的可执行文件：

~~~sh
cd tutorial
make
~~~

### 1.4 编译小工具

tools目录下会默认编译出srpc_tools：

~~~sh
cd tools
make
~~~

小工具用法参考：[srpc/tools/README_cn.md](srpc/tools/README_cn.md)

## 2. Debian Linux和Ubuntu自带安装包

SRPC已经打包到Debian，目前是Debian sid(unstable)的自带安装包，最终会进入stable的repository。

需要编辑/etc/apt/sources.list文件，才能访问unstable的repository：

sources.list文件格式: `deb <respository server/mirror> <repository name> <sub branches of the repo>`

将'unstable'字段加到我们的repo中，作为sub branch之一:
~~~~sh
deb http://deb.debian.org/ main contrib non-free 

--> 

deb http://deb.debian.org/ unstable main contrib non-free
~~~~

然后update我们的repo列表，就可以安装SRPC：

~~~~sh
sudo apt-get update
~~~~

安装SRPC库用于开发：

~~~~sh
sudo apt-get install libsrpc-dev
~~~~

安装SRPC库用于部署：

~~~~sh
sudo apt-get install libsrpc
~~~~

## 3. Fedora Linux自带安装包

SRPC已经是Fedora系统的自带安装包。

为了开发目的安装srpc库：
~~~sh
sudo dnf install srpc-devel
~~~

要安装srpc库以进行部署，请执行以下操作：
~~~sh
sudo dnf install srpc
~~~

## 4. Windows下安装

Windows版下srpc代码无差异，注意需要依赖Workflow的[windows分支](https://github.com/sogou/workflow/tree/windows)。

另外，srpc_tools暂时不支持Windows下使用。

## 5. MacOS源码安装

- 安装依赖 `OpenSSL`
   ```
   brew install openssl
   ```
   
- 安装 `CMake`
   ```
   brew install cmake
   ```

- 指定 `OpenSSL` 环境变量  
    由于MacOS下默认有LibreSSL，因此在brew安装后，并不会自动建软链，我们需要手动把执行路径、编译路径、cmake时的find_package路径都配置到bash的环境变量中。用户可以执行`brew info openssl`查看相关信息，也可以如下配置：
   ```
   echo 'export PATH="/usr/local/opt/openssl@1.1/bin:$PATH"' >> ~/.bash_profile
   echo 'export LDFLAGS="-L/usr/local/opt/openssl@1.1/lib"' >> ~/.bash_profile
   echo 'export CPPFLAGS="-I/usr/local/opt/openssl@1.1/include"' >> ~/.bash_profile
   echo 'export PKG_CONFIG_PATH="/usr/local/opt/openssl@1.1/lib/pkgconfig"' >> ~/.bash_profile
   echo 'export OPENSSL_ROOT_DIR=/usr/local/opt/openssl' >> ~/.bash_profile
   echo 'export OPENSSL_LIBRARIES=/usr/local/opt/openssl/lib' >> ~/.bash_profile
   ```
   如果使用zsh，则还需要以下一步，把bash的配置加载一下：
   ```
   echo 'test -f ~/.bash_profile  && source ~/.bash_profile' >> ~/.zshrc
   source ~/.zshrc
   ```
剩下的步骤和Linux下编译没有区别。
