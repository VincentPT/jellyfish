cmake_minimum_required (VERSION 3.2)
project(cryptos)

# add cpprestsdk interface project
add_subdirectory(cpprestsdk)
# trading base module
add_subdirectory(trading-base)
# trading base module
add_subdirectory(common)
# Bitfinex trading platform module
add_subdirectory(bitfinex)
# HitBTC trading platform module
add_subdirectory(hitbtc)
# binance trading platform module
add_subdirectory(binance)

# sample trading application
add_subdirectory(GoCrypto)