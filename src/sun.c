#include <stdio.h>
#include <math.h>

#include "sun.h"

static float
day_of_year(int year, int month, int day)
{
  float N1 = floor(275 * month / 9.0f);
  float N2 = floor((month + 9) / 12.0f);
  float N3 = (1 + floor((year - 4 * floor(year / 4.0f) + 2) / 3.0f));
  float N = N1 - (N2 * N3) + day - 30;
  return N;
}


static float sindeg(float x)  { return sinf(x * M_PI / 180.0f); }
static float cosdeg(float x)  { return cosf(x * M_PI / 180.0f); }
static float tandeg(float x)  { return tanf(x * M_PI / 180.0f); }
static float atandeg(float x) { return atanf(x) * 180.0f / M_PI; }
static float asindeg(float x) { return asinf(x) * 180.0f / M_PI; }
static float acosdeg(float x) { return acosf(x) * 180.0f / M_PI; }


static float
normalize_degree(float x)
{
  if(x >= 360) {
    while(x >= 360)
      x -= 360;
  } else if(x < 0) {
    while(x < 0)
      x += 360;
  }
  return x;
}


static float
normalize_hour(float x)
{
  if(x >= 24) {
    while(x >= 24)
      x -= 24;
  } else if(x < 0) {
    while(x < 0)
      x += 24;
  }
  return x;
}

//const int mdays[12] = {0, 31,28,31,30,31,30,31,31,30,31,30};


int
suntime(int rising, int year, int month, int day)
{
  const float N = day_of_year(year, month, day);

  const float zenith = 90.5;
  const float longitude = 18.0649;
  const float latitude = 59.3325800;


  // 2. convert the longitude to hour value and calculate an approximate time

  float lngHour = longitude / 15.0;
  
  float t;

  if(rising)
    t = N + ((6 - lngHour) / 24.0);
  else
    t = N + ((18 - lngHour) / 24.0);

  // 3. calculate the Sun's mean anomaly

  float M = (0.9856 * t) - 3.289;

  // 4. calculate the Sun's true longitude

  float L = M + (1.916 * sindeg(M)) + (0.020 * sindeg(2 * M)) + 282.634;
  L = normalize_degree(L);


  // 5a. calculate the Sun's right ascension

  float RA = atandeg(0.91764 * tandeg(L));
  RA = normalize_degree(RA);

  // 5b. right ascension value needs to be in the same quadrant as L

  float Lquadrant  = (floor( L/90.0)) * 90;
  float RAquadrant = (floor(RA/90.0)) * 90;
  RA = RA + (Lquadrant - RAquadrant);

  // 5c. right ascension value needs to be converted into hours

  RA = RA / 15.0;

  // 6. calculate the Sun's declination

  float sinDec = 0.39782 * sindeg(L);
  float cosDec = cosdeg(asindeg(sinDec));

  // 7a. calculate the Sun's local hour angle

  float cosH = (cosdeg(zenith) - (sinDec * sindeg(latitude))) / (cosDec * cosdeg(latitude));

  if(cosH >  1) {
    return -1;
  }
  if(cosH <  -1) {
    return -1;
  }

  float H;

  if(rising)
    H = 360 - acosdeg(cosH);
  else
    H = acosdeg(cosH);

  H = H / 15.0f;

  float T = H + RA - (0.06571 * t) - 6.622;

  float UT = T - lngHour;
  UT = normalize_hour(UT);
  return UT * 60;
}


