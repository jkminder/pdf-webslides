#include "webslides.h"

#include <cairo-svg.h>
#include <cairo.h>
#include <poppler-document.h>
#include <poppler-page.h>
#include <poppler.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cli.h"
#include "colorprint_header.h"
#include "res.h"
#include "utils.h"

#if defined(__linux__) || defined(__linux) || defined(__unix__) || defined(LINUX) || defined(UNIX)
#define LINUX
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__CYGWIN__)
#define WINDOWS
#undef LINUX
#endif

#if defined(WINDOWS)  
#include <shlwapi.h>
#include <windows.h>

void winsystem(const char* app, char* arg) {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags |= STARTF_USESTDHANDLES;
  si.hStdInput = NULL;
  si.hStdError = NULL;
  si.hStdOutput = NULL;
  ZeroMemory(&pi, sizeof(pi));

  CreateProcess(app, arg, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}
#endif

#define NO_SLIDES 0

// ---------------------------------------------------------------------------
static getopt_arg_t cli_options[] = {
    {"single", no_argument, NULL, 's', "Create a single file", NULL},
    {"output", required_argument, NULL, 'o', "Output file name", "FILENAME"},
    {"disablenotes", no_argument, NULL, 'n', "Do not include notes", NULL},
    {"compress", required_argument, NULL, 'c', "Use an SVG compressor (e.g., svgcleaner)", "BINARY"},
    {"version", no_argument, NULL, 'v', "Show version", NULL},
    {"help", no_argument, NULL, 'h', "Show this help.", NULL},
    {"thumbnail_scale", no_argument, NULL, 't', "Thumbnail scale, must be between 0.1 and 1.0 (default 0.3)", "SCALE"},
    {"png", no_argument, NULL, 'p', "Use PNG instead of SVG", NULL},
    {"slide_width", no_argument, NULL, 'w', "Slide width in pixels, only valid together with --png option (default 1920)", "WIDTH"},
    {NULL, 0, NULL, 0, NULL, NULL}};

// ---------------------------------------------------------------------------
void create_thumbnail(PopplerPage* page, const char* fname, int width, int height, float scale) {
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, scale * width, scale * height);
    cairo_t* cr = cairo_create(surface);
    cairo_scale(cr, scale, scale);
    cairo_save(cr);
    poppler_page_render(page, cr);
    cairo_restore(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OVER);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    cairo_destroy(cr);
    cairo_surface_write_to_png(surface, fname);

    cairo_surface_destroy(surface);
}

// ---------------------------------------------------------------------------
int convert(PopplerPage *page, const char *fname, SlideInfo *info, Options *options) {
  double width, height;
  float thumb_scale = options->thumbnail_scale;
  char *comm = calloc(1024, 1);
  char fname_prev[256];
  snprintf(fname_prev, 256, "%s.prev.png", fname);

  poppler_page_get_size(page, &width, &height);
  
  if (options->png) {
    float scale = options->slide_width / width;
    thumb_scale = scale * options->thumbnail_scale;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, scale * width, scale * height);
    cairo_t* cr = cairo_create(surface);
  
    cairo_scale(cr, scale, scale);
    cairo_save(cr);
    poppler_page_render(page, cr);
    cairo_restore(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OVER);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    cairo_destroy(cr);
    cairo_surface_write_to_png(surface, fname);

    cairo_surface_destroy(surface);


  } else {
    cairo_surface_t *surface = cairo_svg_surface_create(fname, width, height);
    cairo_t *img = cairo_create(surface);
    poppler_page_render_for_printing(page, img);
    cairo_show_page(img);
    cairo_destroy(img);
    cairo_surface_destroy(surface);
  }

  info->videos_pos = strdup("");
  info->videos = strdup("");

  GList *annot_list = poppler_page_get_annot_mapping(page);
  GList *s;
  for (s = annot_list; s != NULL; s = s->next) {
    PopplerAnnotMapping *m = (PopplerAnnotMapping *)s->data;
    int type = poppler_annot_get_annot_type(m->annot);
    if (type == 1) {
      char *cont = poppler_annot_get_contents(m->annot);
      if (cont) {
        strncat(comm, cont, 1024);
        strncat(comm, "\n", 1024);
        g_free(cont);
      }
    } else if(type == 19) {
        PopplerMovie *movie = poppler_annot_movie_get_movie((PopplerAnnotMovie*)m->annot);
        if(movie) {
          char *movie_filename = strdup(poppler_movie_get_filename(movie));
          append_elem(&info->videos, movie_filename, "|");

          PopplerRectangle *rectangle = poppler_rectangle_new();
          poppler_annot_get_rectangle(m->annot, rectangle);
          double llx = rectangle->x1 / width;
          double lly = rectangle->y1 / height;
          double urx = rectangle->x2 / width;
          double ury = rectangle->y2 / height;

          size_t bbox_buffer_size =
              snprintf(NULL, 0, "%f;%f;%f;%f", llx, lly, urx, ury) + 1;
          char *bbox_buffer = calloc(bbox_buffer_size, 1);
          sprintf(bbox_buffer, "%f;%f;%f;%f", llx, lly, urx, ury);
          append_elem(&info->videos_pos, bbox_buffer, "|");

          free(movie_filename);
          free(bbox_buffer);
          poppler_rectangle_free(rectangle);
        }
    }
  }

  info->annotations = comm;
  poppler_page_free_annot_mapping(annot_list);

  GList *link_list = poppler_page_get_link_mapping(page);
  for (s = link_list; s != NULL; s = s->next) {
    PopplerLinkMapping *m = (PopplerLinkMapping *)s->data;
    PopplerAction *a = m->action;
    if (a->type == POPPLER_ACTION_LAUNCH) {
      PopplerActionLaunch *launch = (PopplerActionLaunch *)a;
      char *movie_filename = strdup(launch->file_name);
      append_elem(&info->videos, movie_filename, "|");
      append_elem(&info->videos_pos, "0;0;1;1", "|");

      free(movie_filename);
    }
  }
  poppler_page_free_link_mapping(link_list);

  create_thumbnail(page, fname_prev, width, height, thumb_scale);
  return 0;
}

// ---------------------------------------------------------------------------
void progress_cb(int slide) { 
    (void)slide;
    progress_update(1); 
}

// ---------------------------------------------------------------------------
void extract_slide(PopplerDocument *pdffile, int p, SlideInfo *info,
                   Options *options) {
  PopplerPage *page;
  char fname[64], fname_c[64], fname_p[128];
  if (options->png) {
    sprintf(fname, "slide-%d.png", p);
  } else {
    sprintf(fname, "slide-%d.svg", p);
  }
  sprintf(fname_p, "%s.prev.png", fname);
  page = poppler_document_get_page(pdffile, p);
  convert(page, fname, &(info[p]), options);
  if (options->nonotes) {
    free(info[p].annotations);
    info[p].annotations = "";
  }
  if (options->compress) {
    sprintf(fname_c, "slidec-%d.svg", p);
    char convert_cmd[256];
#if defined(WINDOWS)
    sprintf(convert_cmd, "\"%s\" %s %s", options->compress, fname, fname_c);
    winsystem(options->compress, convert_cmd);
#endif
#if defined(LINUX)
    sprintf(convert_cmd, "\"%s\" %s -o %s --multipass 2> /dev/null > /dev/null", options->compress, fname, fname_c);
    system(convert_cmd);
#endif
  } else {
    strcpy(fname_c, fname);
  }
  progress_update(1);
#if NO_SLIDES
  char *b64 = strdup(empty_img);
#else
  char *b64 = encode_file_base64(fname_c);
#endif
  
  info[p].slide = b64;
  info[p].thumb = encode_file_base64(fname_p);
  
  unlink(fname);
  unlink(fname_p);
  if (options->compress) {
      unlink(fname_c);
  }
}

// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  Options options = {.single = 0, .presenter = 0, .nonotes = 0, .name = NULL, .compress = NULL, .thumbnail_scale = 0.3, .png = 0, .slide_width = 1920};
  PopplerDocument *pdffile;
  char abspath[PATH_MAX];
  char fname_uri[PATH_MAX + 32];
  
  if (argc <= 1) {
    show_usage(argv[0], cli_options);
    return 1;
  }
  if (parse_cli_options(&options, cli_options, argc, argv)) {
    return 1;
  }

  const char *input = argv[argc - 1];

  if (access(input, F_OK) == -1) {
    printf_color(1, TAG_FAIL "Could not open file '%s'\n", input);
    return 1;
  }

#if defined(LINUX)
  realpath(input, abspath);
  snprintf(fname_uri, sizeof(fname_uri), "file://%s", abspath);
#elif defined(WINDOWS)
  GetFullPathName(input, PATH_MAX, abspath, NULL);
  unsigned int urllen = PATH_MAX;
  fname_uri[0] = 0;
  UrlCreateFromPath(abspath, fname_uri, &urllen, 0);
#endif
  
  pdffile = poppler_document_new_from_file(fname_uri, NULL, NULL);
  if (pdffile == NULL) {
    printf_color(1, TAG_FAIL "Could not open file '%s'\n", fname_uri);
    return -3;
  }

  int pages = poppler_document_get_n_pages(pdffile);
  printf_color(1, TAG_OK "Loaded %d slides\n", pages);

  char fname[1024];
  snprintf(fname, sizeof(fname), "%s.html",
           options.name ? options.name : "index");
  FILE *output = fopen(fname, "w");
  if (!output) {
    printf_color(1,
                 TAG_FAIL "Could not create output file [m]index.html[/m]\n");
    return 1;
  }
  printf_color(1, TAG_INFO "Converting slides...\n");

  progress_start(1, (pages + 1) * 6 - 1, NULL);

  char *template = read_file("index.html.template"); // /strdup((char*)index_html_template); //

  char* img_black = encode_array_base64((char *)black_svg, black_svg_len);
  char* img_freeze = encode_array_base64((char *)freeze_svg, freeze_svg_len);
  char* img_open = encode_array_base64((char *)open_svg, open_svg_len);
  template = replace_string_first(
      template, "{{black.svg}}", img_black);
  template = replace_string_first(
      template, "{{freeze.svg}}", img_freeze);
  template =
      replace_string_first(template, "{{open.svg}}", img_open);
  free(img_black);
  free(img_freeze);
  free(img_open);
  SlideInfo info[pages + 1];
  memset(info, 0, sizeof(info));

  // create slide data
  for (int p = 0; p < pages; p++) {
    extract_slide(pdffile, p, info, &options);
  }

  info[pages].annotations = strdup("");
  info[pages].slide = strdup("");
  info[pages].videos = strdup("");
  info[pages].thumb = strdup("");
  info[pages].videos_pos = strdup("");

  char *slide_data = encode_array(info, 2, pages + 1, 0, progress_cb);
  char *thumb_data = encode_array(info, 3, pages + 1, 0, progress_cb);
  char *video_pos_data = encode_array(info, 4, pages + 1, 0, progress_cb);
  char *video_data = encode_array(info, 1, pages + 1, 1, progress_cb);
  char *annot_data = encode_array(info, 0, pages + 1, 1, progress_cb);

  if (options.single) {
    template = replace_string_first(template, "{{script}}",
                                    "<script type='text/javascript'>\n"
                                    "var slide_info = {"
                                    "'slides': {{slides}},\n"
                                    "'videos': {{videos}},\n"
                                    "'videos_pos': {{videos_pos}},\n"
                                    "'annotations': {{annotations}},\n"
                                    "'thumb': {{thumb}}\n"
                                    "};\n"
                                    "</script>");

    template = replace_string_first(template, "{{slides}}", slide_data);
    template = replace_string_first(template, "{{videos}}", video_data);
    template = replace_string_first(template, "{{videos_pos}}", video_pos_data);
    template = replace_string_first(template, "{{annotations}}", annot_data);
    template = replace_string_first(template, "{{thumb}}", thumb_data);
  } else {
    char include[1024];
    snprintf(include, sizeof(include),
             "<script type='text/javascript' src='%s.js'></script>\n",
             options.name ? options.name : "slides");
    template = replace_string_first(template, "{{script}}", include);
    snprintf(include, sizeof(include), "%s.js",
             options.name ? options.name : "slides");
    FILE *f = fopen(include, "w");
    fprintf(f,
            "var slide_info = {'slides': %s,\n'videos': %s,\n'videos_pos': %s,\n'annotations': "
            "%s\n, 'thumb': %s\n};\n",
            slide_data, video_data, video_pos_data, annot_data, thumb_data);
    fclose(f);
  }

  template = replace_string_first(template, "{{presenter}}",
                                  options.presenter ? "true" : "false");

  template = replace_string_first(template, "{{slide_data_type}}",
                                  options.png ? "image/png" : "image/svg+xml");

  fwrite(template, strlen(template), 1, output);
  fclose(output);

  printf_color(1, TAG_OK "Done!\n");
  
  for (int p = 0; p <= pages; p++) {
    free(info[p].annotations);
    free(info[p].slide);
    free(info[p].videos);
    free(info[p].thumb);
    free(info[p].videos_pos);
  }
  free(template);
    
  // free(video_pos_data);
  // free(video_data);
  free(thumb_data);
  free(slide_data);
  free(annot_data);

  return 0;
}
