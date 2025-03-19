# Network Speed Test

A lightweight, cross-platform application for measuring your internet connection's performance metrics including ping, download, and upload speeds with real-time graphical feedback.

![Network Speed Test Screenshot](./image.jpg)

## Features

- 📊 Measures ping latency (ms)
- ⬇️ Tests download speed (Mbps)
- ⬆️ Tests upload speed (Mbps)
- 📈 Displays real-time graphical performance during tests
- 🖥️ Clean, modern interface with progress indicators
- 🚀 Fast and lightweight with minimal system resource usage

## Requirements

This project requires SDL2, SDL2_ttf, and cURL libraries.

### macOS Dependencies

You can install the required libraries using Homebrew:

```bash
brew install sdl2 sdl2_ttf curl
```

## Building and Running

Simply clone the repository and use the included Makefile:

```bash
git clone https://github.com/ADJB1212/SpeedTest.git
cd SpeedTest
make
```

The application will automatically run after compilation.

To clean up build artifacts:

```bash
make clean
```

## How It Works

The application uses libcurl to perform network requests against various speed test servers:

1. **Ping Test**: Measures the round-trip time to the server
2. **Download Test**: Downloads a large file and measures the achieved speed
3. **Upload Test**: Uploads generated data and measures the achieved speed

All tests are run on a separate thread to keep the UI responsive, with real-time progress and speed graphs displayed during testing.

## Font Requirements

The application uses Arial font for rendering text. Make sure to have `arial.ttf` in the same directory as the executable, or modify the font path in the source code to use a different font.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
