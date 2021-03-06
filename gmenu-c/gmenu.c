#define _GNU_SOURCE
#include <assert.h>
#include <fnmatch.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

typedef gboolean (*MatchFunc)( const char*, const char* );

typedef struct App {
  gint exit_status;
  gint queue_refilter_id;
  gulong entry_on_changed_id;
  
  MatchFunc match_fn;
  GtkWidget *entry;
  GtkWidget *tree_view;
  GtkTreeModel *filter;
} App;

static gboolean submatch_casefold
( const char *search, const char *subject ) {
    gboolean result;
    gchar *searchi = g_utf8_casefold( search, -1);
    gchar *subjecti = g_utf8_casefold( subject, -1 );
    result = g_strstr_len( subjecti, -1, searchi ) != NULL;
    g_free( searchi );
    g_free( subjecti );
    return result;
}

static gboolean fnmatch_casefold
( const char *search, const char *subject ) {
  return fnmatch( search, subject, FNM_CASEFOLD ) == 0;
}

static gboolean do_refilter
( App *app ) {
  gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER(app->filter) );
  app->queue_refilter_id = 0;
  return FALSE;
}

static void queue_refilter
( App *app ) {
  if( app->queue_refilter_id )
    g_source_remove( app->queue_refilter_id );

  app->queue_refilter_id = g_timeout_add( 300, (GSourceFunc)do_refilter, app );
}

static gboolean visible_func
( GtkTreeModel *model, GtkTreeIter *iter, App *app ){
  const gchar *search = gtk_entry_get_text( GTK_ENTRY(app->entry) );
  gchar *item = NULL;
  gboolean result = FALSE;

  if ( g_utf8_strlen( search, -1 ) == 0 ) {
    result = TRUE;
  }
  else {
    gtk_tree_model_get( model, iter, 0, &item, -1 );
    result = app->match_fn( search, item );
  }

  g_free( item );

  return result;
}

static void entry_on_changed
( GtkEditable *entry, App *app )
{
  queue_refilter( app );
}
/**
 * Set itr_last to last item of parent if parent has any rows.
 * If parent is NULL, set itr_last to last root row.
 * Returns TRUE if itr_last could be set, otherwise FALSE.
 */
static gboolean model_get_iter_last
( GtkTreeModel *model, GtkTreeIter *itr_last, GtkTreeIter *parent )
{
  gboolean success;
  gint n;

  if ( (n = gtk_tree_model_iter_n_children( model, parent ) ) ) {
    success = gtk_tree_model_iter_nth_child( model, itr_last, parent, n - 1 );
  }
  else {
    itr_last = NULL;
    success = FALSE;
  }
  return success;
}

static void select_next
( App *app )
{
  GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(app->tree_view) );
  GtkTreeModel *filter;
  GtkTreeIter i;
  gboolean iter_ok = FALSE;
  if ( gtk_tree_selection_get_selected( sel, &filter, &i ) ) {
    iter_ok = gtk_tree_model_iter_next( filter, &i );
  }
  
  if ( !iter_ok ) {
    iter_ok = gtk_tree_model_get_iter_first( filter, &i );
  }
  
  if ( iter_ok ) {
    gtk_tree_selection_select_iter( sel, &i );
  }
}
static void select_previous
( App *app )
{
  GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(app->tree_view) );
  GtkTreeModel *filter = NULL;
  GtkTreeIter i;
  if ( gtk_tree_selection_get_selected( sel, &filter, &i ) ) {
    GtkTreePath *p = gtk_tree_model_get_path( filter, &i );
    if ( gtk_tree_path_prev( p ) ) {
      gtk_tree_selection_select_path( sel, p );
    }
    else if ( model_get_iter_last( filter, &i, NULL ) ) {
      gtk_tree_selection_select_iter( sel, &i );
    }
    gtk_tree_path_free( p );
  }
  else if ( model_get_iter_last( filter, &i, NULL ) ) {
    gtk_tree_selection_select_iter( sel, &i );
  }
}
static void print_result( App *app ) {
  GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(app->tree_view) );
  GtkTreeModel *filter;
  GtkTreeIter i;
  if ( gtk_tree_selection_get_selected( sel, &filter, &i ) ) {
    gchar *text;
    gtk_tree_model_get( filter, &i, 0, &text,  -1);
    puts( text );
  }
  else {
    puts( gtk_entry_get_text( GTK_ENTRY(app->entry) ) );
  }
}

static gboolean entry_on_key_press
( GtkWidget *w, GdkEventKey *e, App *app )
{
  gboolean tab_pressed = (e->keyval == GDK_Tab);
  gboolean shift_tab_pressed = (
    e->state & GDK_SHIFT_MASK
    && e->keyval == GDK_ISO_Left_Tab
  );
  if ( e->keyval == GDK_Escape ) {
    gtk_main_quit();
    return TRUE;
  }
  else if ( tab_pressed || e->keyval == GDK_Down ) {
    select_next( app );
    return TRUE;
  }
  else if ( shift_tab_pressed || e->keyval == GDK_Up ) {
    select_previous( app );
    return TRUE;
  }
  else if ( e->keyval == GDK_Return ) {
    print_result( app );
    gtk_main_quit();
    return TRUE;
  }
  return FALSE;
}

static void selection_changed
( GtkTreeSelection *sel, App *app ) {
  GtkTreeIter i;
  GtkTreeModel *filter;
  gchar *text;
  if ( gtk_tree_selection_get_selected( sel, &filter, &i ) ) {
    gtk_tree_model_get( filter, &i, 0, &text,  -1);
    GtkTreePath *p = gtk_tree_model_get_path( filter, &i );
    gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW(app->tree_view), p, NULL, FALSE, 0.0, 0.0 );
    gtk_tree_path_free( p );

    g_signal_handler_block( G_OBJECT(app->entry), app->entry_on_changed_id );
    gtk_entry_set_text( GTK_ENTRY(app->entry), text );
    gtk_editable_select_region( GTK_EDITABLE(app->entry), 0, -1 );
    g_signal_handler_unblock( G_OBJECT(app->entry), app->entry_on_changed_id );
  }
}

static void fill_model
( GtkListStore *store )
{
  gchar buffer[4096];
  gchar *line;
  GtkTreeIter iter;
  
  while ( NULL != ( line = fgets( buffer, 4096, stdin ) ) ) {
    gtk_list_store_append( store, &iter );
    gtk_list_store_set( store, &iter, 0, g_strstrip(line), -1 );
  }
}

static void cellrenderer_data_func
( GtkTreeViewColumn *col, GtkCellRenderer *cell, GtkTreeModel *mdl, GtkTreeIter *i, gpointer udata )
{
  App *app = udata;
  gchar *item = NULL;
  gtk_tree_model_get( mdl, i, 0, &item, -1 );
#if 0
  const gchar *search = gtk_entry_get_text( GTK_ENTRY(app->entry) );
  gint search_len = g_utf8_strlen( search, -1 );
  if ( search_len ) {
    gchar *searchi = g_utf8_casefold( search, -1 );
    gchar *itemi = g_utf8_casefold( item, -1 );
    gchar *leadi = g_strstr_len( itemi, -1, searchi );
    if ( leadi ) {
      gint item_len = g_utf8_strlen( item, -1 );
      glong ofs = g_utf8_pointer_to_offset( itemi, leadi );
      gint markup_len = strlen( item ) + 7 + 1;
      gchar *markup = g_malloc0( markup_len );
      gchar *p_markup = markup;
      gchar *p_item = g_utf8_offset_to_pointer( item, ofs );
      
      /* copy leading characters */
      g_utf8_strncpy( p_markup, item, ofs );

      /* append start of markup */
      p_markup = g_utf8_offset_to_pointer( p_markup, ofs );
      g_utf8_strncpy( p_markup, "<b>", 3 );

      /* copy match */
      p_markup = g_utf8_offset_to_pointer( p_markup, 3 );
      g_utf8_strncpy( p_markup, p_item, search_len );

      /* append end of markup */
      p_markup = g_utf8_offset_to_pointer( p_markup, search_len );
      g_utf8_strncpy( p_markup, "</b>", 4 );

      /* copy trailing characters */
      p_markup = g_utf8_offset_to_pointer( p_markup, 4 );
      p_item = g_utf8_offset_to_pointer( p_item, search_len );
      ofs = g_utf8_pointer_to_offset( item, p_item );
      g_utf8_strncpy( p_markup, p_item, item_len - ofs );

      g_object_set( G_OBJECT(cell), "markup", markup, NULL );
      g_free( markup );
    }
    else {
      g_object_set( G_OBJECT(cell), "markup", item, NULL );
    }
    g_free( searchi );
    g_free( itemi );
  }
  else {
    g_object_set( G_OBJECT(cell), "markup", item, NULL );
  }
#endif
  g_object_set( G_OBJECT(cell), "text", item, NULL );
  g_free( item );
}

int main
( int argc, char **argv ) {
  App app;
  app.exit_status = 1;
/*  app.match_fn = fnmatch_casefold; */
  app.match_fn = submatch_casefold;

  gtk_init( &argc, &argv );

  GtkWidget *window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_title( GTK_WINDOW(window), "gmenu" );
  gtk_container_set_border_width( GTK_CONTAINER(window), 2 );
  gtk_window_set_position( GTK_WINDOW(window), GTK_WIN_POS_CENTER );
  gtk_window_set_default_size( GTK_WINDOW(window), 240, 320 );
  gtk_window_set_type_hint( GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG );

  g_signal_connect( G_OBJECT(window), "destroy",
    G_CALLBACK(gtk_main_quit), NULL );

  GtkWidget *vbox = gtk_vbox_new( FALSE, 2 );
  gtk_container_add( GTK_CONTAINER(window), vbox );

  app.entry = gtk_entry_new();
  app.entry_on_changed_id = g_signal_connect( G_OBJECT(app.entry), "changed",
    G_CALLBACK(entry_on_changed), &app );
  g_signal_connect( G_OBJECT(app.entry), "key-press-event",
    G_CALLBACK(entry_on_key_press), &app );
  gtk_box_pack_start( GTK_BOX(vbox), app.entry, FALSE, FALSE, 0 );

  GtkWidget *swindow = gtk_scrolled_window_new( NULL, NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(swindow),
    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
  gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(swindow),
    GTK_SHADOW_IN );
  gtk_box_pack_start( GTK_BOX(vbox), swindow, TRUE, TRUE, 0 );

  GtkListStore *store = gtk_list_store_new( 1, G_TYPE_STRING );
  fill_model( store );

  app.filter = gtk_tree_model_filter_new( GTK_TREE_MODEL(store), NULL );
  g_object_unref( G_OBJECT(store) );
  gtk_tree_model_filter_set_visible_func( GTK_TREE_MODEL_FILTER(app.filter),
    (GtkTreeModelFilterVisibleFunc)visible_func, &app, NULL );

  app.tree_view = gtk_tree_view_new_with_model( app.filter );
  gtk_tree_view_set_headers_visible( GTK_TREE_VIEW(app.tree_view), FALSE );
  GtkTreeSelection *sel = gtk_tree_view_get_selection(
    GTK_TREE_VIEW(app.tree_view) );
  gtk_tree_selection_set_mode( sel, GTK_SELECTION_SINGLE );
  g_signal_connect( G_OBJECT(sel), "changed",
    G_CALLBACK(selection_changed), &app );
  
  gtk_container_add( GTK_CONTAINER(swindow), app.tree_view );
  g_object_unref( G_OBJECT(app.filter) );

  GtkCellRenderer *cell = gtk_cell_renderer_text_new();
  g_object_set( G_OBJECT(cell),
    "ellipsize", PANGO_ELLIPSIZE_START, NULL );
  gtk_tree_view_insert_column_with_data_func( GTK_TREE_VIEW(app.tree_view),
    -1, "Name", cell, cellrenderer_data_func, &app, NULL );

  gtk_widget_show_all( window );
  gtk_main();
  
  return app.exit_status;
}
