#pragma once

class Timer
{
  LARGE_INTEGER lFreq, lStart;
  LONGLONG elapsedTime;

public:
  Timer()
  {
    QueryPerformanceFrequency(&lFreq);
    elapsedTime = 0;
  }

  inline void start()
  {
    QueryPerformanceCounter(&lStart);
  }
  
  inline void stop()
  {
    LARGE_INTEGER lEnd;
    QueryPerformanceCounter(&lEnd);
    elapsedTime = lEnd.QuadPart - lStart.QuadPart;
  }

  inline double getElapsedTime()
  {
    // Return duration in seconds...
	  LARGE_INTEGER lEnd;
	  QueryPerformanceCounter(&lEnd);
	  elapsedTime = lEnd.QuadPart - lStart.QuadPart;
    return (double(elapsedTime) / lFreq.QuadPart);
  }

  inline double stopAndGetTime()
  {
      //stop();
      return getElapsedTime();
  }
};
