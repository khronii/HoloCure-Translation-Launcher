#include <stdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>



enum
{
  H_NAME = 0,
  H_VERSION,
  H_LINK,
  H_CHKSUM,
  H_N_COLUMNS
};



typedef struct 
{
  gchar *name;
  gchar *version;
  gchar *link;
  gchar *chksum;
} 
version_info;



typedef struct 
{
  GtkWidget *cb;
  GtkWidget *progress;
  GtkWidget *status;

  GtkButton *self;
  const version_info *info;
} button_context;



static GList* ver_lst; //version_info*
static int preferred;



static void
h_start_button_callback (GtkButton* self,
                         gpointer   user_data);

static void
h_start_button_download_status (goffset  current,
                             goffset  total,
                             gpointer user_data);

static void
h_start_button_extract_data (GObject* source_object,
                             GAsyncResult* res,
                             gpointer user_data);

static void
h_start_button_start_game (GPid pid,
                           gint wait_status,
                           gpointer user_data);



static void
to_linux_path (gchar *path)
{
  path[1] = path[0];
  path[0] = '/';
  for(size_t i = 0; path[i] != '\0'; i++)
    if (path[i] == '\\') path[i] = '/';
}



static void
h_xml_start (GMarkupParseContext *context,
             const gchar         *element_name,
             const gchar        **attribute_names,
             const gchar        **attribute_values,
             gpointer             user_data,
             GError             **error)
{
  GList **list = (GList **)user_data;
  version_info* ver = g_new0 (version_info, 1);

  const gchar *temp_name;

  for (const gchar **names = attribute_names, **values = attribute_values;
       *names != NULL;
       names++, values++)
  {
    if      (g_strcmp0 (*names, "name") == 0)
      temp_name =              *values; // *****
    else if (g_strcmp0 (*names, "version") == 0)
      ver->version = g_strdup (*values);
    else if (g_strcmp0 (*names, "link") == 0)
      ver->link    = g_strdup (*values);
    else if (g_strcmp0 (*names, "chksum") == 0)
      ver->chksum  = g_strdup (*values);
  }

  ver->name = g_strdup_printf ("%s (%s)", temp_name, ver->version);

  *list = g_list_append (*list, ver);
}



static void
h_free_version_info (void *ver)
{
  version_info *v = ver;
  g_free (v->name);
  g_free (v->version);
  g_free (v->link);
  g_free (v->chksum);

  g_free (v);
}



static GList*
h_init_version_info_list (int *preferred)
{
  gchar *path = g_build_path ("//", g_getenv ("localappdata"), "HoloCureKR", NULL);
  gchar *versions_xml = g_build_path ("//", path, "versions.xml", NULL);
  //gchar *pref = g_build_path ("//", path, "pref.txt", NULL);
  GFile *file = g_file_new_for_path(path);
  g_file_make_directory (file, NULL, NULL);
  g_object_unref (file);
  g_free (path);

  *preferred = 0; //default - latest
  
/*
  if (g_file_test (prefname, G_FILE_TEST_EXISTS))
  {
    GList *lst = NULL;
    return lst;
  }
  */

  //Download!
  //TODO: ADD CANCELLABLE; g_timeout_add_seconds
  GFile *src = g_file_new_for_uri ("https://raw.githubusercontent.com/khronii/HoloCure-Translation-Launcher/main/versions.xml");
  GFile *dest = g_file_new_for_path (versions_xml);
  g_file_copy (src,dest,G_FILE_COPY_OVERWRITE,NULL,NULL,NULL,NULL);
  g_object_unref (src);
  g_object_unref (dest);

  gchar *buf;
  gsize len;
  g_file_get_contents (versions_xml, &buf, &len, NULL);

  g_free (versions_xml);

  GMarkupParser parser = {
    .start_element = h_xml_start,
    .end_element   = NULL,
    .text          = NULL,
    .passthrough   = NULL,
    .error         = NULL};

  GList *lst = NULL;
  GMarkupParseContext *context = g_markup_parse_context_new(&parser, 0, (gpointer) &lst, NULL);

  if(g_markup_parse_context_parse(context, buf, len, NULL))
  {
    version_info* ver = g_new0 (version_info, 1);
    *ver = (version_info) {
      g_strdup ("최신 빌드"),
      g_strdup ("?.?.?"),
      g_strdup (""),
      g_strdup ("LATEST")
    };
    lst = g_list_prepend (lst, ver);
  }

  g_free(buf);
  
  return lst; //FAIL시 lst == NULL
}



static void
h_free_version_info_list (GList *ver)
{
  g_list_free_full(ver, h_free_version_info);
}



static GdkPixbuf*
h_gdk_pixbuf_scale_width(GdkPixbuf* buf,
                         int        width)
{
  int w = gdk_pixbuf_get_width(buf);
  int h = gdk_pixbuf_get_height(buf);

  return gdk_pixbuf_scale_simple(buf, width, h * width / w, GDK_INTERP_HYPER);
}

static GtkWidget*
h_get_info_label()
{
  GtkWidget* label;

  const gchar *text =
    "게임 공식 홈페이지 : <a href=\"https://kay-yu.itch.io/holocure\">itch.io</a>\n"
    "코딩/번역 : <a href=\"https://github.com/khronii\">[출처_필요]</a>\n"
    "번역 : Flora (<a href=\"https://twitter.com/flora_852\">@flora_852</a>)\n"
    "로고 한글화 : 리노(<a href=\"https://twitter.com/rito__321\">@rito__321</a>)";
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), text);

  return label;
}

static GtkWidget*
h_get_version_combo_box()
{
  GtkWidget *cbox;
  GtkListStore *store = gtk_list_store_new (H_N_COLUMNS,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING);

  GtkTreeIter iter;
  for (GList* elem = ver_lst; elem; elem = elem->next)
  {
    version_info* v = (version_info*) elem->data;
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        H_NAME,    v->name,
                        H_VERSION, v->version,
                        H_LINK,    v->link,
                        H_CHKSUM,  v->chksum,
                        -1);
  }

  cbox = gtk_combo_box_new ();
  gtk_combo_box_set_id_column (GTK_COMBO_BOX(cbox), H_NAME);
  gtk_combo_box_set_model (GTK_COMBO_BOX(cbox), GTK_TREE_MODEL(store));

  gtk_cell_layout_clear (GTK_CELL_LAYOUT(cbox));
  GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT(cbox),
                            text_renderer, TRUE );
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT(cbox), 
                                 text_renderer,
                                 "text",
                                 H_NAME);
  
  gtk_combo_box_set_active(GTK_COMBO_BOX(cbox), 0);

  return cbox;
}

static void
h_start_button_callback (GtkButton* self,
                         gpointer   user_data)
{
  button_context* ctx = user_data;
  const gchar *localappdata = g_getenv ("localappdata");
  int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(ctx->cb));
  sel = (sel <= 0) ? 1 : sel;
  ctx->info = (version_info *) g_list_nth_data (ver_lst, sel);
  ctx->self = self;
  const gchar *link = ctx->info->link;
  gchar *zip_name = g_strconcat(ctx->info->chksum,".zip",NULL);
  gchar *zip_path = g_build_path ("\\", localappdata, "HoloCureKR", zip_name, NULL);

  //LOCK BUTTON HERE
  gtk_widget_set_sensitive (GTK_WIDGET(self), FALSE);

  gboolean no_zip = !g_file_test (zip_path, G_FILE_TEST_EXISTS);
  if (no_zip)
    {
      g_info ("Download!");
      
      gchar *temp_name = g_strconcat(ctx->info->chksum,".zip.part",NULL);
      gchar *temp_path = g_build_path ("\\", localappdata, "HoloCureKR", temp_name, NULL);
      //Download!
      GFile *src = g_file_new_for_uri (link);
      GFile *dest = g_file_new_for_path (temp_path);
      g_file_copy_async (src,dest,G_FILE_COPY_OVERWRITE,G_PRIORITY_DEFAULT_IDLE-10,NULL,
                         h_start_button_download_status,user_data,
                         h_start_button_extract_data,user_data);
      g_object_unref (src);
      g_object_unref (dest);
      g_free (temp_name);
      g_free (temp_path);
    }

  g_free(zip_name);
  g_free(zip_path);

  if (!no_zip)
    h_start_button_extract_data (0, 0, user_data);
}

static void
h_start_button_download_status (goffset  current,
                                goffset  total,
                                gpointer user_data)
{
  button_context* ctx = user_data;

  if (current != total)
  {
    gchar *progress = g_strdup_printf ("%" G_GINT64_FORMAT " / %" G_GINT64_FORMAT "(%.1f%%)", (gint64) current, (gint64) total, (float)current/(float)total*100);
    gtk_button_set_label (ctx->self, progress);
    g_free (progress);

    return;
  }
  else
    gtk_button_set_label (ctx->self, "Start!");
}

static void
h_start_button_extract_data (GObject* source_object,
                             GAsyncResult* res,
                             gpointer user_data)
{
  button_context* ctx = user_data;

  const gchar *localappdata = g_getenv ("localappdata");
  gchar *zip_name = g_strconcat(ctx->info->chksum,".zip",NULL);
  gchar *zip_path = g_build_path ("\\", localappdata, "HoloCureKR", zip_name, NULL);
  gchar *game_path = g_build_path ("\\", localappdata, "HoloCureKR", ctx->info->chksum, NULL);
  gchar *game_exe  = g_build_path ("\\", game_path, "HoloCure.exe", NULL);

  gchar *temp_name = g_strconcat(ctx->info->chksum,".zip.part",NULL);
  gchar *temp_path = g_build_path ("\\", localappdata, "HoloCureKR", temp_name, NULL);
  if (g_file_test (temp_path, G_FILE_TEST_EXISTS))
  {
    GFile *temp = g_file_new_for_path (temp_path);
    GFile *zip  = g_file_new_for_path (zip_path);
    g_file_move (temp, zip, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);
    g_object_unref (temp);
    g_object_unref (zip);
  }
  g_free (temp_name);
  g_free (temp_path);

  gboolean no_game = !g_file_test (game_exe, G_FILE_TEST_EXISTS);
  if (no_game)
    {
      g_info ("Extract!");

      //Extract!
      GFile *file = g_file_new_for_path (game_path);
      g_file_make_directory (file, NULL, NULL);
      g_object_unref(file);

      GPid pid;

      to_linux_path (zip_path);
      to_linux_path (game_path);
      gchar *unzip_arg[] = {"unzip", "-a", zip_path, "-d", game_path, NULL};
      g_spawn_async (NULL, unzip_arg, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL);
      g_child_watch_add_full (G_PRIORITY_DEFAULT_IDLE, pid, h_start_button_start_game, user_data, NULL);
    }
  
  g_free(zip_name);
  g_free(zip_path);
  g_free(game_path);
  g_free(game_exe);

  if (!no_game)
    h_start_button_start_game(0, 0, user_data);
}

static void
h_start_button_start_game (GPid pid,
                           gint wait_status,
                           gpointer user_data)
{
  if (pid != 0)
    g_spawn_close_pid (pid);
  button_context* ctx = user_data;
  const gchar *localappdata = g_getenv ("localappdata");
  gchar *game_path = g_build_path ("\\", localappdata, "HoloCureKR", ctx->info->chksum, NULL);
  gchar *game_exe  = g_build_path ("\\", game_path, "HoloCure.exe", NULL);
  
  gchar *game_arg[] = {game_exe, NULL};
  g_spawn_async (game_path, game_arg, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, NULL, NULL);

  //TODO: unlock button as async callback
  gtk_widget_set_sensitive (GTK_WIDGET(ctx->self), TRUE);

  g_free(game_path);
  g_free(game_exe);
}

static void
activate (GtkApplication* app,
          gpointer        user_data)
{
  GtkWidget *window;
  GtkWidget *grid;
  GdkPixbuf *image_buf;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "HoloCure 한글패치 전용 런쳐");
  //gtk_window_set_default_size (GTK_WINDOW (window), 800, 680);

  grid = gtk_grid_new();
  gtk_container_add (GTK_CONTAINER(window), grid);
  
  if ((image_buf = gdk_pixbuf_new_from_file("holocure.png",NULL)) != NULL)
  {
    GdkPixbuf *image_scaled_buf = h_gdk_pixbuf_scale_width(image_buf, 700);
    GtkWidget *image = gtk_image_new_from_pixbuf(image_scaled_buf);
    g_object_unref(image_buf);
    g_object_unref(image_scaled_buf);
    
    gtk_grid_attach(GTK_GRID(grid), image, 0,0, 2,1);
  }
  else
  {
    g_info ("Image load failed");
  }

  GtkWidget *cb = h_get_version_combo_box();
  GtkWidget *start_button = gtk_button_new_with_label("Start!");
  button_context *ctx = g_new(button_context, 1);
  *ctx = (button_context) {.cb=cb, .progress = NULL, .status = NULL};
  g_signal_connect_data (start_button, 
                         "clicked", 
                         G_CALLBACK(h_start_button_callback), 
                         (gpointer) ctx,
                         (GClosureNotify) g_free, //It works on my computer(R)
                         0);

  gtk_grid_attach(GTK_GRID(grid), h_get_info_label(), 0,1, 2,1);
  gtk_grid_attach(GTK_GRID(grid), cb, 0,4, 1,1);
  gtk_grid_attach(GTK_GRID(grid), start_button, 1,4, 1,1);

  gtk_widget_show_all (window);
}

int
main (int    argc,
      char **argv)
{
  GtkApplication *app;
  int status;

  ver_lst = h_init_version_info_list (&preferred);

  app = gtk_application_new ("kr.holocure.launcher", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  h_free_version_info_list (ver_lst);

  return status;
}
