#include "utility.h"
#include "data_structures.h"
#include "data_io.h"
#include "d8_methods.h"
#include "dinf_methods.h"
#include "pit_fill.h"
#include "interface.h"
#include "flat_resolution.h"
#include "debug.h"
#include <string>
#include <sys/time.h>


/**
  @brief  Calculates Jacob's Wetland Metric
  @author Richard Barnes, Jacob Galzki

  @param[in]  &smoothed_cti
    CTI generated with d8_CTI() and smoothed with array2d::low_pass_filter()
  @param[in]  &hydric_soils
    A grid containing a [0,2] representation of hydric soils. 0=no hydric soil presence, 1=partial presence, and 2=all hydric soils.
  @param[in]  &smoothed_percent_slope
    Percent Slope grid generated by d8_slope() with argument #TATTRIB_SLOPE_PERCENT and smoothed with array2d::low_pass_filter()
  @param[in]  &smoothed_profile_curvature
    Profile curvature grid generated by d8_profile_curvature() and smoothed with array2d::low_pass_filter()
  @param[out]  &result
    Hold's Jacob's Wetland Metric: a probability of a given cell being part of a wetland

  @pre All grids should be the same size

  @post \pname{result} takes the properties and dimensions of \pname{smoothed_cti}

  @todo Ensure all grids are the same size
*/

int main(int argc, char **argv){
  Timer running_calc_time,running_io_time,total_time;

  total_time.start();

  float_2d elevations;

  running_io_time.start();
  load_ascii_data(argv[1],elevations);
  running_io_time.stop();

  running_calc_time.start();
  elevations.low_pass_filter();
  barnes_flood(elevations);

  float_2d flowdirs(elevations);
  dinf_flow_directions(elevations,flowdirs);

  int_2d flat_resolution_mask, groups;
  resolve_flats_barnes(elevations,flowdirs,flat_resolution_mask,groups);
  dinf_flow_flats(flat_resolution_mask,groups,flowdirs);
  flat_resolution_mask.clear();
  groups.clear();

  float_2d area;
  dinf_upslope_area(flowdirs, area);
  flowdirs.clear();

  float_2d percent_slope;
  d8_slope(elevations, percent_slope, TATTRIB_SLOPE_PERCENT);

  float_2d cti;
  d8_CTI(area, percent_slope, cti);
  area.clear();

  float_2d linear_regressor(elevations);
  linear_regressor.init(-4);
  linear_regressor.no_data=-99999.123456;

  cti.low_pass_filter();
  linear_regressor+=cti*0.5;
  cti.clear();

  percent_slope.low_pass_filter();
  linear_regressor+=percent_slope*-0.2;
  percent_slope.clear();

  float_2d profile_curvature;
  d8_profile_curvature(elevations, profile_curvature);
  elevations.clear();

  profile_curvature.low_pass_filter();
  linear_regressor+=profile_curvature*-0.5;
  profile_curvature.clear();

  char_2d hydric_soils;
  load_ascii_data(argv[2], hydric_soils);

  linear_regressor+=hydric_soils;
  hydric_soils.clear();

/*  std::cout<<"#######CTI##########"<<std::endl;
  cti.print_random_sample(50);
  std::cout<<"#######Percent Slope##########"<<std::endl;
  percent_slope.print_random_sample(50);
  std::cout<<"#######Curvature##########"<<std::endl;
  profile_curvature.print_random_sample(50);
*/

  diagnostic("Jacob's Wetland Metric...");
  #pragma omp parallel for collapse(2)
  for(int x=0;x<linear_regressor.width();x++)
  for(int y=0;y<linear_regressor.height();y++)
    if(linear_regressor(x,y)!=linear_regressor.no_data)
      linear_regressor(x,y)=1/(1+pow(EULER_CONST,-linear_regressor(x,y)));
  diagnostic("succeeded.\n");

  linear_regressor.low_pass_filter();

  running_calc_time.stop();

  running_io_time.start();
  output_ascii_data(argv[3], linear_regressor);
  running_io_time.stop();


  running_calc_time.start();
  long hist[1000];
  for(int i=0;i<1000;i++)
    hist[i]=0;

  for(int x=0;x<linear_regressor.width();x++)
  for(int y=0;y<linear_regressor.height();y++)
    if(linear_regressor(x,y)!=linear_regressor.no_data)
      hist[(int)(linear_regressor(x,y)*1000)]++;

  for(int i=0;i<1000;i++)
    diagnostic_arg("JProb %d %ld\n",i,hist[i]);
  running_calc_time.stop();


  total_time.stop();

  diagnostic_arg("Total time was: %lfs\n", total_time.accumulated());
  diagnostic_arg("Time spent in computation: %lfs\n",running_calc_time.accumulated());
  diagnostic_arg("Time spent in I/O: %lfs\n",running_io_time.accumulated());

  return 0;
}
