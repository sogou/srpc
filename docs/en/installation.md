# Installation

## 1. Install from source code on Linux

### 1.1 Dependencies

SRPC depends on the following modules :
* Manually install : **CMake**(Require >= v3.6.0), **OpenSSL**(Recommend >= v1.1.0), **Protobuf**(Require >= v3.5.0)
* Automatically install : **Workflow**, **snappy**, **lz4**  

Workflow, snappy and lz4 can also be found via installed package in the system. If the submodule dependencies are not pulled in third\_party by '--recursive', they will be searched from the default installation path of the system. The version of snappy is required v1.1.6 or above.

To install **Protobuf** by source code (Please ignore it if Protobuf is installed):

~~~sh
git clone -b 3.20.x https://github.com/protocolbuffers/protobuf.git protobuf.3.20
cd protobuf.3.20
sh autogen.sh
./configure
make -j4
make install
~~~

### 1.2 Compile and Install SRPC

Supports 'make', 'bazel' and 'cmake'. Compiling SRPC will get the following output: 
- static library: libsrpc.a (or dylib)
- dynamic library: libsrpc.so (or dll)
- tool for generating code: srpc_generator

Just choose one of the following three commands:

- **make**
   ~~~sh
   git clone --recursive https://github.com/sogou/srpc.git
   cd srpc
   make
   ~~~

   Execute the following command to install to the system default path (optional. Or you need to pay attention to specify the path to find the SRPC library and header files when building your own code):
   ~~~sh
   make install
   ~~~

- **bazel**
   ~~~sh
   bazel build ...
   # It can compile out lib and src generator and all tutorials into bazel-bin/
   ~~~

- **cmake**
   ~~~sh
   mkdir build.cmake
   cd build.cmake
   cmake ..
   make
   ~~~

### 1.3 Compile tutorial

~~~sh
cd tutorial
make
~~~

### 1.4 Compile srpc_tools

~~~sh
cd tools
make
~~~

Please refer to the usage：[srpc/tools/README.md](srpc/tools/README.md)

## 2. Debian Linux

SRPC has been packaged for Debian. It is currently in Debian sid (unstable) but will eventually be placed into the stable repository.

In order to access the unstable repository, you will need to edit your /etc/apt/sources.list file.

sources.list has the format: `deb <respository server/mirror> <repository name> <sub branches of the repo>`

Simply add the 'unstable' sub branch to your repo:
~~~~sh
deb http://deb.debian.org/ main contrib non-free 

--> 

deb http://deb.debian.org/ unstable main contrib non-free
~~~~

Once that is added, update your repo list and then you should be able to install it:
~~~~sh
sudo apt-get update
~~~~

To install the srpc library for development purposes:
~~~~sh
sudo apt-get install libsrpc-dev
~~~~

To install the srpc library for deployment:
~~~~sh
sudo apt-get install libsrpc
~~~~

## 3. Fedora Linux

SRPC has been packaged for Fedora.

To install the srpc library for development purposes:
~~~~sh
sudo dnf install srpc-devel
~~~~

To install the srpc library for deployment:
~~~~sh
sudo dnf install srpc
~~~~

## 4. Windows

There is no difference in the srpc code under the Windows version, but users need to use the [windows branch](https://github.com/sogou/workflow/tree/windows) of Workflow

Moreover, srpc_tools is not supported on Windows.

## 5. MacOS

- Install dependency `OpenSSL`
   ```
   brew install openssl
   ```
   
- Install `CMake`
   ```
   brew install cmake
   ```

- Specify the `OpenSSL` environment variable  
    The soft link of OpenSSL will not be automatically built after installation by brew, because LibreSSL is provided by default under MacOS. We need to manually configure the execution path, compilation path, and find_package path into the environment variables of cmake. You can execute `brew info openssl` to view relevant information, or configure it as follows:
   ```
   echo 'export PATH="/usr/local/opt/openssl@1.1/bin:$PATH"' >> ~/.bash_profile
   echo 'export LDFLAGS="-L/usr/local/opt/openssl@1.1/lib"' >> ~/.bash_profile
   echo 'export CPPFLAGS="-I/usr/local/opt/openssl@1.1/include"' >> ~/.bash_profile
   echo 'export PKG_CONFIG_PATH="/usr/local/opt/openssl@1.1/lib/pkgconfig"' >> ~/.bash_profile
   echo 'export OPENSSL_ROOT_DIR=/usr/local/opt/openssl' >> ~/.bash_profile
   echo 'export OPENSSL_LIBRARIES=/usr/local/opt/openssl/lib' >> ~/.bash_profile
   ```
   If you use `zsh`, you need one more step to load the bash configuration:
   ```
   echo 'test -f ~/.bash_profile  && source ~/.bash_profile' >> ~/.zshrc
   source ~/.zshrc
   ```
The remaining steps are no different from compiling on Linux.
