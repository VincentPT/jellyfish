# jellyfish
A crypto application to help people in trading by showing and analyzing crypto's information.

## Getting Started
[Application binaries - v1.0.0.zip](/binaries/v1.0.0.zip)

Application screen.  
![Application screen](/docs/GoCrypto-with-marks.jpg)

Mobile notification.  
![Notification](/docs/notifications.jpg)

1. platform name(platform's config file name without extension)  
platform name list is loaded in forlder ./platforms in binaries folder.
       
2. Market global data.
    
3. Message notification through PushOver platform.
    
4. Platform currencies.  
the currencies which use as base currencies to exchange to other cryocurencies.  
For example: USD, BTC, ETH,... in Bitfinex trading platform.  
       
5. Graph settings.  
Control graph length and bar chart length.

6. Symbol filter
       
7. Log filter  
    
8. Log level  
    
9. Log messages area  
    
10. Charts  
* Bart chart for volume.  
* Line chart for price.  
       
11. Average prices.  
Average prices for specific periods.
       
12. Total volume.  
Total volume of base symbol exchanged for specific periods.  

13. Buy Per Shell.  
Ratio in percent of bought volume per sold volume.

15. Notification.  
Notification through PushOver platform.

### Prerequisites
#### Windows
* Visual studio 2012, 2015. 
* CMake 3.2 or above
* Casablanca C++ REST SDK 2.9.2
* Cinder 0.9 or above
* Imgui
* Cinder-ImGui
    
#### Linux
* Not supported yet
    
 #### MacOS
* Not supported yet

## Run.
- The binaries package is already to run.  
  Howerver, notification feature is only available if the PushOver's keys is specifed in platform's configuration file.  
  if you don't have a PushOver's account, you can register for trial [here](https://pushover.net/).  
    
- In case you want to run application with Binance's services you should supply your API key in platform's configuration file too.
    
- The platform's configuration file can be found in platforms folder in the binaries package.
## Authors
* **Vincent Pham** - [VincentPT](https://github.com/VincentPT)

## License
This project is licensed under the MIT License 
