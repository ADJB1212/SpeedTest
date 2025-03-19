#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 400
#define MAX_POINTS 100

typedef struct {
  double downloadSpeed;
  double uploadSpeed;
  double ping;
  int isTesting;
  int testComplete;
  int currentStage;
  char statusText[256];
  double progress;
  double downloadPoints[MAX_POINTS];
  double uploadPoints[MAX_POINTS];
  int numDownloadPoints;
  int numUploadPoints;
} SpeedTestData;

struct DownloadData {
  size_t totalBytes;
  double startTime;
  double lastUpdate;
  SpeedTestData *testData;
};

struct UploadData {
  const char *data;
  size_t size;
  size_t position;
  double startTime;
  size_t totalUploaded;
  SpeedTestData *testData;
};

typedef struct {
  char *memory;
  size_t size;
  struct timeval startTime;
  size_t totalBytes;
  SpeedTestData *testData;
  int isDownload;
  int streamID;
} MemoryStruct;

static char uploadData[2000000];
static int uploadDataInitialized = 0;

void renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                SDL_Color color, SDL_Rect rect);
void renderProgressBar(SDL_Renderer *renderer, double progress, int x, int y,
                       int width, int height);
void renderGraph(SDL_Renderer *renderer, double *points, int numPoints, int x,
                 int y, int width, int height, int max_y);
void testPing(SpeedTestData *testData);
void testDownloadSpeed(SpeedTestData *testData);
void testUploadSpeed(SpeedTestData *testData);
static size_t writeMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp);
static size_t readMemoryCallback(void *ptr, size_t size, size_t nmemb,
                                 void *userp);
double getTimeMs(void);

int speedTestThread(void *data) {
  SpeedTestData *testData = (SpeedTestData *)data;

  testData->isTesting = 1;
  testData->testComplete = 0;
  testData->progress = 0.0;
  testData->numDownloadPoints = 0;
  testData->numUploadPoints = 0;

  curl_global_init(CURL_GLOBAL_ALL);

  testData->currentStage = 1;
  strcpy(testData->statusText, "Testing ping...");
  testPing(testData);
  testData->progress = 0.25;

  testData->currentStage = 2;
  strcpy(testData->statusText, "Testing download speed...");
  testDownloadSpeed(testData);
  testData->progress = 0.6;

  testData->currentStage = 3;
  strcpy(testData->statusText, "Testing upload speed...");
  testUploadSpeed(testData);
  testData->progress = 1.0;

  curl_global_cleanup();

  testData->currentStage = 4;
  strcpy(testData->statusText, "Test complete");
  testData->isTesting = 0;
  testData->testComplete = 1;

  return 0;
}

int main(int argc, char *argv[]) {
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  TTF_Font *font = NULL;
  TTF_Font *smallFont = NULL;
  TTF_Font *authorFont = NULL;
  SDL_Event event;
  int quit = 0;
  SpeedTestData testData = {0};
  SDL_Thread *testThread = NULL;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n",
            SDL_GetError());
    return 1;
  }

  if (TTF_Init() < 0) {
    fprintf(stderr, "SDL_ttf could not initialize! TTF_Error: %s\n",
            TTF_GetError());
    SDL_Quit();
    return 1;
  }

  window = SDL_CreateWindow("Network Speed Test", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH,
                            WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  if (!window) {
    fprintf(stderr, "Window could not be created! SDL_Error: %s\n",
            SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n",
            SDL_GetError());
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  font = TTF_OpenFont("arial.ttf", 24);
  authorFont = TTF_OpenFont("arial.ttf", 19);
  smallFont = TTF_OpenFont("arial.ttf", 16);
  if (!font || !smallFont) {
    fprintf(stderr, "Failed to load font! TTF_Error: %s\n", TTF_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  SDL_Rect buttonRect = {WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT - 60, 200, 50};

  while (!quit) {

    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        quit = 1;
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        int x, y;
        SDL_GetMouseState(&x, &y);

        if (!testData.isTesting && x >= buttonRect.x &&
            x <= buttonRect.x + buttonRect.w && y >= buttonRect.y &&
            y <= buttonRect.y + buttonRect.h) {

          testThread =
              SDL_CreateThread(speedTestThread, "SpeedTest", &testData);
          SDL_DetachThread(testThread);
        }
      }
    }

    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderClear(renderer);

    SDL_Color darkBlue = {0, 80, 160, 255};
    SDL_Rect titleRect = {WINDOW_WIDTH / 2 - 150, 5, 300, 40};
    renderText(renderer, font, "Network Speed Test", darkBlue, titleRect);

    SDL_Color darkGrey = {63, 67, 70, 255};
    SDL_Rect authorRect = {WINDOW_WIDTH / 2 - 200, 30, 400, 40};
    renderText(renderer, authorFont, "by Andrew Jaffe", darkGrey, authorRect);

    SDL_Color gray = {100, 100, 100, 255};
    SDL_Rect statusRect = {WINDOW_WIDTH / 2 - 150, 55, 300, 30};
    renderText(renderer, smallFont, testData.statusText, gray, statusRect);

    if (testData.isTesting) {
      renderProgressBar(renderer, testData.progress, 100, 85,
                        WINDOW_WIDTH - 200, 20);
    }

    SDL_Color black = {0, 0, 0, 255};
    SDL_Rect resultsBg = {50, 120, WINDOW_WIDTH - 100, 160};
    SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255);
    SDL_RenderFillRect(renderer, &resultsBg);
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderDrawRect(renderer, &resultsBg);

    char pingText[100];
    if (testData.testComplete) {
      sprintf(pingText, "Ping: %.2f ms", testData.ping);
    } else {
      sprintf(pingText, "Ping: --");
    }
    SDL_Rect pingRect = {70, 130, 200, 30};
    renderText(renderer, font, pingText, black, pingRect);

    char downloadText[100];
    if (testData.testComplete) {
      sprintf(downloadText, "Download: %.2f Mbps", testData.downloadSpeed);
    } else {
      sprintf(downloadText, "Download: --");
    }
    SDL_Rect downloadRect = {20, 170, 250, 30};
    renderText(renderer, font, downloadText, black, downloadRect);

    char uploadText[100];
    if (testData.testComplete) {
      sprintf(uploadText, "Upload: %.2f Mbps", testData.uploadSpeed);
    } else {
      sprintf(uploadText, "Upload: --");
    }
    SDL_Rect uploadRect = {40, 210, 300, 30};
    renderText(renderer, font, uploadText, black, uploadRect);

    if (testData.numDownloadPoints > 0) {
      renderGraph(renderer, testData.downloadPoints, testData.numDownloadPoints,
                  300, 170, 240, 30, 100);
    }

    if (testData.numUploadPoints > 0) {
      renderGraph(renderer, testData.uploadPoints, testData.numUploadPoints,
                  300, 210, 240, 30, 100);
    }

    SDL_SetRenderDrawColor(renderer, 0, 120, 200, 255);
    SDL_RenderFillRect(renderer, &buttonRect);

    SDL_Rect button_border = buttonRect;
    SDL_SetRenderDrawColor(renderer, 0, 80, 160, 255);
    SDL_RenderDrawRect(renderer, &button_border);

    SDL_Color white = {255, 255, 255, 255};
    renderText(renderer, font, testData.isTesting ? "Testing..." : "Start Test",
               white, buttonRect);

    SDL_RenderPresent(renderer);

    SDL_Delay(16);
  }

  TTF_CloseFont(font);
  TTF_CloseFont(smallFont);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();

  return 0;
}

void renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                SDL_Color color, SDL_Rect rect) {
  SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
  if (!surface) {
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (!texture) {
    SDL_FreeSurface(surface);
    return;
  }

  SDL_Rect destRect = rect;
  if (surface->w < rect.w) {
    destRect.x = rect.x + (rect.w - surface->w) / 2;
    destRect.w = surface->w;
  }
  if (surface->h < rect.h) {
    destRect.y = rect.y + (rect.h - surface->h) / 2;
    destRect.h = surface->h;
  }

  SDL_RenderCopy(renderer, texture, NULL, &destRect);

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

void renderProgressBar(SDL_Renderer *renderer, double progress, int x, int y,
                       int width, int height) {

  SDL_Rect bgRect = {x, y, width, height};
  SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
  SDL_RenderFillRect(renderer, &bgRect);

  int progressWidth = (int)(width * progress);
  if (progressWidth > 0) {
    SDL_Rect progress_rect = {x, y, progressWidth, height};
    SDL_SetRenderDrawColor(renderer, 0, 180, 0, 255);
    SDL_RenderFillRect(renderer, &progress_rect);
  }

  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderDrawRect(renderer, &bgRect);
}

void renderGraph(SDL_Renderer *renderer, double *points, int numPoints, int x,
                 int y, int width, int height, int max_y) {

  SDL_Rect bgRect = {x, y, width, height};
  SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
  SDL_RenderFillRect(renderer, &bgRect);

  SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
  SDL_RenderDrawRect(renderer, &bgRect);

  if (numPoints <= 1)
    return;

  SDL_SetRenderDrawColor(renderer, 0, 150, 220, 255);

  int pointSpacing = width / (numPoints > 1 ? (numPoints - 1) : 1);

  for (int i = 0; i < numPoints - 1; i++) {
    int x1 = x + i * pointSpacing;
    int y1 = y + height - (int)((points[i] / max_y) * height);
    if (y1 < y)
      y1 = y;
    if (y1 > y + height)
      y1 = y + height;

    int x2 = x + (i + 1) * pointSpacing;
    int y2 = y + height - (int)((points[i + 1] / max_y) * height);
    if (y2 < y)
      y2 = y;
    if (y2 > y + height)
      y2 = y + height;

    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
  }
}

double getTimeMs(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

static size_t writeMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
  size_t realsize = size * nmemb;
  MemoryStruct *mem = (MemoryStruct *)userp;

  mem->totalBytes += realsize;

  if (mem->isDownload && mem->testData) {
    struct timeval now;
    gettimeofday(&now, NULL);

    double timeDiff = (now.tv_sec - mem->startTime.tv_sec) * 1000.0 +
                      (now.tv_usec - mem->startTime.tv_usec) / 1000.0;

    if (timeDiff > 100.0) {
      double mbps = (mem->totalBytes * 8.0 / 1000000.0) / (timeDiff / 1000.0);

      if (mem->streamID == 0 && mem->testData->numDownloadPoints < MAX_POINTS) {
        mem->testData->downloadPoints[mem->testData->numDownloadPoints++] =
            mbps;
      }
    }
  }

  return realsize;
}

static size_t readMemoryCallback(void *ptr, size_t size, size_t nmemb,
                                 void *userp) {
  MemoryStruct *mem = (MemoryStruct *)userp;
  size_t maxSize = size * nmemb;

  char *data = (char *)ptr;
  for (size_t i = 0; i < maxSize; i++) {
    data[i] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rand() % 36];
  }

  mem->totalBytes += maxSize;

  if (mem->testData) {
    struct timeval now;
    gettimeofday(&now, NULL);

    double timeDiff = (now.tv_sec - mem->startTime.tv_sec) * 1000.0 +
                      (now.tv_usec - mem->startTime.tv_usec) / 1000.0;

    if (timeDiff > 100.0) {
      double mbps = (mem->totalBytes * 8.0 / 1000000.0) / (timeDiff / 1000.0);

      if (mem->streamID == 0 && mem->testData->numUploadPoints < MAX_POINTS) {
        mem->testData->uploadPoints[mem->testData->numUploadPoints++] = mbps;
      }
    }
  }

  return maxSize;
}

void testPing(SpeedTestData *testData) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    testData->ping = 0;
    return;
  }

  char pingURL[512];
  snprintf(pingURL, sizeof(pingURL), "%s%s?r=%d",
           "https://librespeed.org/backend", "/empty.php", rand());

  curl_easy_setopt(curl, CURLOPT_URL, pingURL);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  double totalTime = 0;
  int successfulPings = 0;

  for (int i = 0; i < 5; i++) {
    double startTime = getTimeMs();
    CURLcode res = curl_easy_perform(curl);
    double endTime = getTimeMs();

    if (res == CURLE_OK) {
      double timeSpent = endTime - startTime;
      totalTime += timeSpent;
      successfulPings++;
    }

    SDL_Delay(200);
  }

  if (successfulPings > 0) {
    testData->ping = totalTime / successfulPings;
  } else {
    testData->ping = 0;
  }

  curl_easy_cleanup(curl);
}

static size_t writeCallback(char *ptr, size_t size, size_t nmemb,
                            void *userdata) {

  struct DownloadData *download = (struct DownloadData *)userdata;
  size_t realSize = size * nmemb;

  download->totalBytes += realSize;

  double now = getTimeMs();
  double elapsed = (now - download->startTime) / 1000.0;

  if (elapsed > 0 && download->testData->numDownloadPoints < MAX_POINTS) {
    if (now - download->lastUpdate > 200) {
      double mbps = (download->totalBytes * 8 / 1000000.0) / elapsed;
      download->testData
          ->downloadPoints[download->testData->numDownloadPoints++] = mbps;
      download->lastUpdate = now;
    }
  }

  return realSize;
}

static size_t readCallback(char *buffer, size_t size, size_t nitems,
                           void *userdata) {

  struct UploadData *upload = (struct UploadData *)userdata;
  size_t bufferSize = size * nitems;

  if (upload->position >= upload->size) {

    upload->position = 0;
  }

  size_t toCopy = upload->size - upload->position;
  if (toCopy > bufferSize) {
    toCopy = bufferSize;
  }

  memcpy(buffer, upload->data + upload->position, toCopy);
  upload->position += toCopy;
  upload->totalUploaded += toCopy;

  double now = getTimeMs();
  double elapsed = (now - upload->startTime) / 1000.0;

  if (elapsed > 0 && upload->testData->numUploadPoints < MAX_POINTS) {
    if (upload->testData->numUploadPoints == 0 ||
        now - upload->startTime > upload->testData->numUploadPoints * 200) {
      double mbps = (upload->totalUploaded * 8 / 1000000.0) / elapsed;
      upload->testData->uploadPoints[upload->testData->numUploadPoints++] =
          mbps;
    }
  }

  return toCopy;
}

void testDownloadSpeed(SpeedTestData *testData) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    testData->downloadSpeed = 0;
    return;
  }

  const char *downloadURL =
      "https://speed.cloudflare.com/__down?bytes=100000000";

  struct DownloadData download = {0, getTimeMs(), 0, testData};

  testData->numDownloadPoints = 0;

  curl_easy_setopt(curl, CURLOPT_URL, downloadURL);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

  double startTime = getTimeMs();
  CURLcode res = curl_easy_perform(curl);
  double endTime = getTimeMs();
  double testDuration = (endTime - startTime) / 1000.0;

  if (res != CURLE_OK) {

    const char *fallbackURL = "http://speedtest.tele2.net/100MB.zip";

    download.totalBytes = 0;
    download.startTime = getTimeMs();
    download.lastUpdate = 0;
    testData->numDownloadPoints = 0;

    curl_easy_setopt(curl, CURLOPT_URL, fallbackURL);

    startTime = getTimeMs();
    res = curl_easy_perform(curl);
    endTime = getTimeMs();
    testDuration = (endTime - startTime) / 1000.0;

    if (res != CURLE_OK) {

      if (download.totalBytes > 0 && testDuration > 0) {

        testData->downloadSpeed =
            (download.totalBytes * 8 / 1000000.0) / testDuration;
      } else {
        testData->downloadSpeed = 0;
      }
    } else {
      if (download.totalBytes > 0 && testDuration > 0) {
        testData->downloadSpeed =
            (download.totalBytes * 8 / 1000000.0) / testDuration;
      } else {
        testData->downloadSpeed = 0;
      }
    }
  } else {
    if (download.totalBytes > 0 && testDuration > 0) {
      testData->downloadSpeed =
          (download.totalBytes * 8 / 1000000.0) / testDuration;
    } else {
      testData->downloadSpeed = 0;
    }
  }

  curl_easy_cleanup(curl);
}

static size_t discardResponse(char *ptr, size_t size, size_t nmemb,
                              void *userdata) {

  return size * nmemb;
}

void testUploadSpeed(SpeedTestData *testData) {

  if (!uploadDataInitialized) {
    for (int i = 0; i < sizeof(uploadData); i++) {
      uploadData[i] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rand() % 36];
    }
    uploadDataInitialized = 1;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    testData->uploadSpeed = 0;
    return;
  }

  const char *uploadURL = "https://httpbin.org/post";

  testData->numUploadPoints = 0;

  struct UploadData upload = {uploadData, sizeof(uploadData), 0, getTimeMs(), 0,
                              testData};

  curl_easy_setopt(curl, CURLOPT_URL, uploadURL);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &upload);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 20000000);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

  double startTime = getTimeMs();
  CURLcode res = curl_easy_perform(curl);
  double endTime = getTimeMs();
  double testDuration = (endTime - startTime) / 1000.0;

  if (res != CURLE_OK) {

    if (upload.totalUploaded > 0 && testDuration > 0) {

      testData->uploadSpeed =
          (upload.totalUploaded * 8 / 1000000.0) / testDuration;

    } else {
      testData->uploadSpeed = 0;
    }
  } else {
    if (upload.totalUploaded > 0 && testDuration > 0) {
      testData->uploadSpeed =
          (upload.totalUploaded * 8 / 1000000.0) / testDuration;

      curl_easy_pause(curl, CURLPAUSE_ALL);
    } else {
      testData->uploadSpeed = 0;
    }
  }

  curl_easy_cleanup(curl);
}
