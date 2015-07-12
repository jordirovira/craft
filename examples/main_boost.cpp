
#include <test_lib.h>
#include <boost/system/error_code.hpp>
#include <iostream>

using namespace boost::system;

void fail(error_code &ec)
{
  ec = errc::make_error_code(errc::not_supported);
}

int main()
{
    std::cout << get_a() << get_b() << std::endl;

    error_code ec;
    fail(ec);
    std::cout << ec.value() << '\n';

    return 0;
}
