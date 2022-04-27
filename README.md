# Non blocking servers

## How to run?

### Install dependencies
We need `c++ 17` to compile these servers, because of toml++ dependency requireed for parsing configuration files.
```console
git submodule init
git submodule update
```
### Compile a server
Go to the respective folder, and run `make`.

### Configure the server
Open `config.toml` file. All required fields must be there, and cannot be ommitted. You can choose to remove fileds under optional category.
