#ifdef __unix__
#define _POSIX_C_SOURCE 200112L
#endif

/* ######################### includes ############################################################################### */

#include <math.h>
#include <float.h>
#include <limits.h>

#include "args_int.h"
#include "interaction_int.h"
#include "plot_int.h"
#include "util_int.h"
#include "gr.h"


/* ========================= macros ================================================================================= */

/* ------------------------- math ----------------------------------------------------------------------------------- */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ######################### public implementation ################################################################## */

/* ========================= methods ================================================================================ */

/* ------------------------- user interaction ----------------------------------------------------------------------- */

int grm_input(const grm_args_t *input_args)
{
  /*
   * reset_ranges:
   * - `x`, `y`: mouse cursor position
   * - `key`: Pressed key (as string)
   * zoom:
   * - `x`, `y`: start point
   * - `angle_delta`: mouse wheel rotation angle in eighths of a degree, can be replaced by factor (double type)
   * - `factor`: zoom factor, can be replaced by angle_delta (double type)
   * box zoom:
   * - `x1`, `y1`, `x2`, `y2`: coordinates of a box selection, (x1, y1) is the fixed corner
   * - `keep_aspect_ratio`: if set to `1`, the aspect ratio of the gr window is preserved (defaults to `1`)
   * pan:
   * - `x`, `y`: start point
   * - `xshift`, `yshift`: shift in x and y direction
   *
   * All coordinates are expected to be given as workstation coordinates (integer type)
   */
  int width, height, max_width_height;
  int x, y, x1, y1, x2, y2;
  grm_args_t *subplot_args;
  const double *viewport;
  double viewport_mid_x, viewport_mid_y;

  logger((stderr, "Processing input\n"));

  get_figure_size(NULL, &width, &height, NULL, NULL);
  max_width_height = grm_max(width, height);
  logger((stderr, "Using size (%d, %d)\n", width, height));

  if (grm_args_values(input_args, "x", "i", &x) && grm_args_values(input_args, "y", "i", &y))
    {
      double ndc_x, ndc_y;
      char *key;

      ndc_x = (double)x / max_width_height;
      ndc_y = (double)(height - y) / max_width_height;
      logger((stderr, "x: %d, y: %d, ndc_x: %lf, ndc_y: %lf\n", x, y, ndc_x, ndc_y));

      subplot_args = get_subplot_from_ndc_point(ndc_x, ndc_y);

      if (grm_args_values(input_args, "key", "s", &key))
        {
          logger((stderr, "Got key \"%s\"\n", key));

          if (strcmp(key, "r") == 0)
            {
              if (subplot_args != NULL)
                {
                  logger((stderr, "Reset single subplot coordinate ranges\n"));
                  grm_args_push(subplot_args, "reset_ranges", "i", 1);
                }
              else
                {
                  grm_args_t **subplot_args_ptr;
                  logger((stderr, "Reset all subplot coordinate ranges\n"));
                  grm_args_values(active_plot_args, "subplots", "A", &subplot_args_ptr);
                  while (*subplot_args_ptr != NULL)
                    {
                      grm_args_push(*subplot_args_ptr, "reset_ranges", "i", 1);
                      ++subplot_args_ptr;
                    }
                }
            }

          return 1;
        }

      if (subplot_args != NULL)
        {
          double angle_delta, factor;
          int xshift, yshift;
          const char *kind;
          grm_args_values(subplot_args, "viewport", "D", &viewport);

          grm_args_values(subplot_args, "kind", "s", &kind);
          if (strcmp(kind, "marginalheatmap") == 0)
            {
              grm_args_t **current_series;
              double *x_series, *y_series;
              unsigned int x_length, y_length;
              double x_0, x_end, y_0, y_end, x_step, y_step;
              double xind, yind;

              grm_args_values(input_args, "x", "i", &x);
              grm_args_values(input_args, "y", "i", &y);
              grm_args_values(subplot_args, "series", "A", &current_series);
              grm_args_first_value(*current_series, "x", "D", &x_series, &x_length);
              grm_args_first_value(*current_series, "y", "D", &y_series, &y_length);

              x_0 = x_series[0], x_end = x_series[x_length - 1];
              y_0 = y_series[0], y_end = y_series[y_length - 1];

              gr_wctondc(&x_0, &y_0);
              gr_wctondc(&x_end, &y_end);
              x_0 = x_0 * max_width_height;
              x_end = x_end * max_width_height;
              y_0 = height - y_0 * max_width_height;
              y_end = height - y_end * max_width_height;

              x_step = (x_end - x_0) / x_length;
              y_step = (y_end - y_0) / y_length;
              xind = (x - x_0) / x_step;
              yind = (y - y_0) / y_step;

              if (xind < 0 || xind >= x_length || yind < 0 || yind >= y_length)
                {
                  xind = -1;
                  yind = -1;
                }
              grm_args_push(subplot_args, "xind", "i", (int)xind);
              grm_args_push(subplot_args, "yind", "i", (int)yind);
            }

          if (grm_args_values(input_args, "angle_delta", "d", &angle_delta))
            {
              double focus_x, focus_y;

              if (str_equals_any(kind, 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume",
                                 "isosurface"))
                {
                  /*
                   * TODO Zoom in 3D
                   */
                }
              else
                {
                  viewport_mid_x = (viewport[0] + viewport[1]) / 2.0;
                  viewport_mid_y = (viewport[2] + viewport[3]) / 2.0;
                  focus_x = ndc_x - viewport_mid_x;
                  focus_y = ndc_y - viewport_mid_y;
                  logger(
                      (stderr, "Zoom to ndc focus point (%lf, %lf), angle_delta %lf\n", focus_x, focus_y, angle_delta));
                  grm_args_push(subplot_args, "panzoom", "ddd", focus_x, focus_y,
                                1.0 - INPUT_ANGLE_DELTA_FACTOR * angle_delta);
                }

              return 1;
            }
          else if (grm_args_values(input_args, "factor", "d", &factor))
            {
              double focus_x, focus_y;

              if (str_equals_any(kind, 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume",
                                 "isosurface"))
                {
                  /*
                   * TODO Zoom in 3D
                   */
                }
              else
                {
                  viewport_mid_x = (viewport[0] + viewport[1]) / 2.0;
                  viewport_mid_y = (viewport[2] + viewport[3]) / 2.0;
                  focus_x = ndc_x - viewport_mid_x;
                  focus_y = ndc_y - viewport_mid_y;
                  logger((stderr, "Zoom to ndc focus point (%lf, %lf), factor %lf\n", focus_x, focus_y, factor));
                  grm_args_push(subplot_args, "panzoom", "ddd", focus_x, focus_y, factor);
                }

              return 1;
            }

          if (grm_args_values(input_args, "xshift", "i", &xshift) &&
              grm_args_values(input_args, "yshift", "i", &yshift))
            {
              double ndc_xshift, ndc_yshift, rotation, tilt;
              int shift_pressed;

              if (str_equals_any(kind, 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume",
                                 "isosurface"))
                {
                  if (grm_args_values(input_args, "shift_pressed", "i", &shift_pressed) && shift_pressed)
                    {
                      /*
                       * TODO Translate in 3D
                       */
                    }
                  else
                    {
                      grm_args_values(subplot_args, "rotation", "d", &rotation);
                      grm_args_values(subplot_args, "tilt", "d", &tilt);

                      rotation += xshift * 0.2;
                      tilt -= yshift * 0.2;

                      if (tilt > 180)
                        {
                          tilt = 180;
                        }
                      else if (tilt < 0)
                        {
                          tilt = 0;
                        }

                      grm_args_push(subplot_args, "rotation", "d", rotation);
                      grm_args_push(subplot_args, "tilt", "d", tilt);
                    }
                }
              else
                {
                  ndc_xshift = (double)-xshift / max_width_height;
                  ndc_yshift = (double)yshift / max_width_height;
                  logger((stderr, "Translate by ndc coordinates (%lf, %lf)\n", ndc_xshift, ndc_yshift));
                  grm_args_push(subplot_args, "panzoom", "ddd", ndc_xshift, ndc_yshift, 0.0);
                }
              return 1;
            }
        }
    }

  if (grm_args_values(input_args, "x1", "i", &x1) && grm_args_values(input_args, "x2", "i", &x2) &&
      grm_args_values(input_args, "y1", "i", &y1) && grm_args_values(input_args, "y2", "i", &y2))
    {
      double focus_x, focus_y, factor_x, factor_y;
      int keep_aspect_ratio = INPUT_DEFAULT_KEEP_ASPECT_RATIO;

      grm_args_values(input_args, "keep_aspect_ratio", "i", &keep_aspect_ratio);

      if (!get_focus_and_factor(x1, y1, x2, y2, keep_aspect_ratio, &factor_x, &factor_y, &focus_x, &focus_y,
                                &subplot_args))
        {
          return 0;
        }

      logger((stderr, "Got widget size: (%d, %d)\n", width, height));
      logger((stderr, "Got box: (%d, %d, %d, %d)\n", x1, y1, x2, y2));
      logger((stderr, "zoom focus: (%lf, %lf)\n", focus_x, focus_y));
      logger((stderr, "zoom factors: (%lf, %lf)\n", factor_x, factor_y));

      grm_args_push(subplot_args, "panzoom", "dddd", focus_x, focus_y, factor_x, factor_y);

      return 1;
    }

  return 0;
}

int grm_is3d(const int x, const int y)
{
  int width, height, max_width_height;
  double ndc_x, ndc_y;
  grm_args_t *subplot_args;
  const char *kind;

  get_figure_size(NULL, &width, &height, NULL, NULL);
  max_width_height = grm_max(width, height);
  ndc_x = (double)x / max_width_height;
  ndc_y = (double)y / max_width_height;

  subplot_args = get_subplot_from_ndc_points(1, &ndc_x, &ndc_y);

  if (subplot_args && grm_args_values(subplot_args, "kind", "s", &kind) &&
      str_equals_any(kind, 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume", "isosurface"))
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

int grm_get_box(const int x1, const int y1, const int x2, const int y2, const int keep_aspect_ratio, int *x, int *y,
                int *w, int *h)
{
  int width, height, max_width_height;
  double focus_x, focus_y, factor_x, factor_y;
  double viewport_mid_x, viewport_mid_y;
  const double *viewport, *wswindow;
  grm_args_t *subplot_args;
  get_figure_size(NULL, &width, &height, NULL, NULL);
  max_width_height = grm_max(width, height);
  if (!get_focus_and_factor(x1, y1, x2, y2, keep_aspect_ratio, &factor_x, &factor_y, &focus_x, &focus_y, &subplot_args))
    {
      return 0;
    }
  grm_args_values(active_plot_args, "wswindow", "D", &wswindow);
  grm_args_values(subplot_args, "viewport", "D", &viewport);
  viewport_mid_x = (viewport[1] + viewport[0]) / 2.0;
  viewport_mid_y = (viewport[3] + viewport[2]) / 2.0;
  *w = (int)grm_round(factor_x * width * (viewport[1] - viewport[0]) / (wswindow[1] - wswindow[0]));
  *h = (int)grm_round(factor_y * height * (viewport[3] - viewport[2]) / (wswindow[3] - wswindow[2]));
  *x = (int)grm_round(((viewport_mid_x + focus_x) - ((viewport_mid_x + focus_x) - viewport[0]) * factor_x) *
                      max_width_height);
  *y = (int)grm_round(height - ((viewport_mid_y + focus_y) - ((viewport_mid_y + focus_y) - viewport[3]) * factor_y) *
                                   max_width_height);
  return 1;
}

grm_tooltip_info_t *grm_get_tooltip(const int mouse_x, const int mouse_y)
{
  grm_tooltip_info_t *info = malloc(sizeof(grm_tooltip_info_t));
  double *x_series, *y_series, *z_series, x, y, x_min, x_max, y_min, y_max, mindiff = DBL_MAX, diff;
  double x_range_min, x_range_max, y_range_min, y_range_max, x_px, y_px;
  int width, height, max_width_height;
  unsigned int num_labels = 0;
  char *kind, **labels;
  grm_args_t *subplot_args, **current_series;
  unsigned int x_length, y_length, z_length, series_i = 0, i;
  char *orientation;
  int is_vertical = 0;

  get_figure_size(NULL, &width, &height, NULL, NULL);
  max_width_height = grm_max(width, height);
  x = (double)mouse_x / max_width_height;
  y = (double)(height - mouse_y) / max_width_height;

  subplot_args = get_subplot_from_ndc_points(1, &x, &y);
  if (subplot_args != NULL)
    {
      grm_args_values(subplot_args, "kind", "s", &kind);
      if (grm_args_values(subplot_args, "orientation", "s", &orientation))
        is_vertical = strcmp(orientation, "vertical") == 0;
    }
  if (subplot_args == NULL || !str_equals_any(kind, 13, "line", "scatter", "stem", "step", "heatmap", "marginalheatmap",
                                              "contour", "imshow", "contourf", "pie", "hexbin", "shade", "quiver"))
    {
      info->x_px = -1;
      info->y_px = -1;
      info->x = 0;
      info->y = 0;
      info->xlabel = "x";
      info->ylabel = "y";
      info->label = "";
      return info;
    }
  plot_process_viewport(subplot_args);
  plot_process_window(subplot_args);

  gr_ndctowc(&x, &y);
  if (!grm_args_values(subplot_args, "xlabel", "s", &info->xlabel))
    {
      info->xlabel = "x";
    }
  if (!grm_args_values(subplot_args, "ylabel", "s", &info->ylabel))
    {
      info->ylabel = "y";
    }

  x_range_min = (double)(mouse_x - 50) / max_width_height;
  x_range_max = (double)(mouse_x + 50) / max_width_height;
  y_range_min = (double)(height - (mouse_y + 50)) / max_width_height;
  y_range_max = (double)(height - (mouse_y - 50)) / max_width_height;
  gr_ndctowc(&x_range_min, &y_range_min);
  gr_ndctowc(&x_range_max, &y_range_max);

  grm_args_values(subplot_args, "series", "A", &current_series);
  grm_args_values(subplot_args, "_xlim", "dd", &x_min, &x_max);
  grm_args_values(subplot_args, "_ylim", "dd", &y_min, &y_max);

  x_range_min = (x_min > x_range_min) ? x_min : x_range_min;
  y_range_min = (y_min > y_range_min) ? y_min : y_range_min;
  x_range_max = (x_max < x_range_max) ? x_max : x_range_max;
  y_range_max = (y_max < y_range_max) ? y_max : y_range_max;
  grm_args_first_value(subplot_args, "labels", "S", &labels, &num_labels);

  if (strcmp(kind, "pie") == 0)
    {
      static char output[50];
      double max_x = 0.95, min_x = 0.05, max_y = 0.05, min_y = 0.95;
      int center_x, center_y;
      double radius;
      double *normalized_x = NULL;
      double start_angle, end_angle, act_angle;

      gr_wctondc(&max_x, &max_y);
      max_x = max_x * max_width_height;
      max_y = height - max_y * max_width_height;
      gr_wctondc(&min_x, &min_y);
      min_x = min_x * max_width_height;
      min_y = height - min_y * max_width_height;

      /* calculate the circle */
      radius = (max_x - min_x) / 2.0;
      center_x = (int)(max_x - radius);
      center_y = (int)(max_y - radius);

      grm_args_first_value(*current_series, "x", "D", &x_series, &x_length);
      normalized_x = normalize(x_length, x_series);
      start_angle = 90.0;
      for (i = 0; i < x_length; ++i)
        {
          end_angle = start_angle - normalized_x[i] * 360.0;
          act_angle =
              acos((mouse_x - center_x) / sqrt(pow(abs(mouse_x - center_x), 2) + pow(abs(mouse_y - center_y), 2)));
          act_angle = (mouse_y - center_y > 0) ? act_angle * 180 / M_PI : 360 - act_angle * 180 / M_PI;
          if (act_angle >= 270 && act_angle <= 360) act_angle = act_angle - 360;
          act_angle *= -1;
          if (start_angle >= act_angle && end_angle <= act_angle)
            {
              snprintf(output, 50, "%f", x_series[i]);
            }
          start_angle = end_angle;
        }

      if (sqrt(pow(abs(mouse_x - center_x), 2) + pow(abs(mouse_y - center_y), 2)) <= radius)
        {
          mindiff = 0;
          info->x = 0;
          info->y = 0;
          info->x_px = mouse_x;
          info->y_px = mouse_y;
          info->label = output;
          return info;
        }
      else
        {
          mindiff = DBL_MAX;
        }
    }

  while (*current_series != NULL && strcmp(kind, "pie") != 0)
    {
      if (is_vertical)
        {
          grm_args_first_value(*current_series, "x", "D", &y_series, &y_length);
          grm_args_first_value(*current_series, "y", "D", &x_series, &x_length);
        }
      else
        {
          grm_args_first_value(*current_series, "x", "D", &x_series, &x_length);
          grm_args_first_value(*current_series, "y", "D", &y_series, &y_length);
        }
      if (str_equals_any(kind, 5, "heatmap", "marginalheatmap", "contour", "imshow", "contourf"))
        {
          grm_args_first_value(*current_series, "z", "D", &z_series, &z_length);
        }
      for (i = 0; i < x_length; i++)
        {
          if ((x_series[i] < x_range_min || x_series[i] > x_range_max || y_series[i] < y_range_min ||
               y_series[i] > y_range_max) &&
              !str_equals_any(kind, 6, "heatmap", "marginalheatmap", "contour", "imshow", "contourf", "quiver"))
            {
              continue;
            }
          x_px = x_series[i];
          y_px = y_series[i];
          gr_wctondc(&x_px, &y_px);
          x_px = (x_px * max_width_height);
          y_px = (height - y_px * max_width_height);
          diff = sqrt((x_px - mouse_x) * (x_px - mouse_x) + (y_px - mouse_y) * (y_px - mouse_y));
          if (diff < mindiff && diff <= 50)
            {
              mindiff = diff;
              info->x = x_series[i];
              info->y = y_series[i];
              info->x_px = (int)x_px;
              info->y_px = (int)y_px;
              if (num_labels > series_i)
                {
                  info->label = labels[series_i];
                }
              else
                {
                  info->label = "";
                }
            }
          else if (str_equals_any(kind, 6, "heatmap", "marginalheatmap", "contour", "imshow", "contourf", "quiver"))
            {
              static char output[50];
              double num;
              double x_0 = x_series[0], x_end = x_series[x_length - 1], y_0 = y_series[0],
                     y_end = y_series[y_length - 1];
              double x_step, y_step, x_series_idx, y_series_idx;
              unsigned int u_length, v_length;
              double *u_series, *v_series;

              if (strcmp(kind, "imshow") == 0) x_0 = x_min, x_end = x_max, y_0 = y_min, y_end = y_max;

              gr_wctondc(&x_0, &y_0);
              gr_wctondc(&x_end, &y_end);
              x_0 = x_0 * max_width_height;
              x_end = x_end * max_width_height;
              y_0 = height - y_0 * max_width_height;
              y_end = height - y_end * max_width_height;

              x_step = (x_end - x_0) / x_length;
              y_step = (y_end - y_0) / y_length;
              if (strcmp(kind, "quiver") == 0)
                {
                  grm_args_first_value(*current_series, "u", "D", &u_series, &u_length);
                  grm_args_first_value(*current_series, "v", "D", &v_series, &v_length);
                }

              mindiff = 0;
              x_series_idx = (mouse_x - x_0) / x_step;
              y_series_idx = (mouse_y - y_0) / y_step;
              if (x_series_idx < 0 || x_series_idx >= x_length || y_series_idx < 0 || y_series_idx >= y_length)
                {
                  mindiff = DBL_MAX;
                  break;
                }
              if (strcmp(kind, "quiver") == 0)
                {
                  info->xlabel = "u";
                  info->ylabel = "v";
                  info->x = u_series[(int)(y_series_idx)*x_length + (int)(x_series_idx)];
                  info->y = v_series[(int)(y_series_idx)*x_length + (int)(x_series_idx)];
                }
              else
                {
                  info->x = x_series[(int)x_series_idx];
                  info->y = y_series[(int)y_series_idx];
                }
              info->x_px = mouse_x;
              info->y_px = mouse_y;

              if (strcmp(kind, "quiver") == 0)
                {
                  info->label = "";
                }
              else
                {
                  num = z_series[(int)y_series_idx * x_length + (int)x_series_idx];
                  snprintf(output, 50, "%f", num);
                  info->label = output;
                }
            }
        }
      ++series_i;
      ++current_series;
    }
  if (mindiff == DBL_MAX)
    {
      info->x_px = -1;
      info->y_px = -1;
      info->x = 0;
      info->y = 0;
      info->label = "";
    }
  return info;
}
