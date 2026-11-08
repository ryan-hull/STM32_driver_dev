I am building out bare metal drivers for my STM32-F401RE developer board.

I am doing this to gain a deeper understanding of how each of the standard protocols (GPIO,
UART, SPI, I2C, etc) work under the hood. I use Claude as a tutor and a guide, and collaborated
with it to generate the initial requirements doc for the project, but the code is written by me
by hand as much as possible. Originally I set out to do this as a pure bare metal project, but
I decided to use the CMSIS headers to speed things along rather than looking up every memory
address and offset in the reference docs.

