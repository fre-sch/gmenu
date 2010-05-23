#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

typedef struct App {
  gint exit_status;
  gint queue_refilter_id;
  gulong entry_on_changed_id;
  
  GtkWidget *entry;
  GtkWidget *tree_view;
  GtkTreeModel *filter;
} App;


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
  const gchar *needle;
  gchar *haystack;
  gboolean result;

  needle = gtk_entry_get_text( GTK_ENTRY(app->entry) );
  if( *needle == '\0' )
      return TRUE;
  gtk_tree_model_get( model, iter, 0, &haystack, -1 );

  result = ( strstr( haystack, needle ) ? TRUE : FALSE );
  g_free( haystack );

  return result;
}

static void entry_on_changed
( GtkEditable *entry, App *app )
{
  queue_refilter( app );
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
  GtkTreePath *p = NULL;
  if ( gtk_tree_selection_get_selected( sel, &filter, &i ) ) {
    p = gtk_tree_model_get_path( filter, &i );
    if ( gtk_tree_path_prev( p ) ) {
      gtk_tree_selection_select_path( sel, p );
    }
    else {
      GtkTreeIter l;
      while ( gtk_tree_model_iter_next( filter, &i) ) {
        l = i;
      }
      gtk_tree_selection_select_iter( sel, &l );
    }
  }
  else {
    GtkTreeIter l;
    if ( gtk_tree_model_get_iter_first( filter, &i ) ) {
      while ( gtk_tree_model_iter_next( filter, &i) ) {
        l = i;
      }
      gtk_tree_selection_select_iter( sel, &l );
    }
  }
  
  gtk_tree_path_free( p );
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
  if ( e->keyval == GDK_Escape ) {
    gtk_main_quit();
    return TRUE;
  }
  else if ( e->keyval == GDK_Tab || e->keyval == GDK_Down ) {
    select_next( app );
    return TRUE;
  }
  else if ( e->keyval == GDK_Up ) {
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

int main
( int argc, char **argv ) {
  App app;
  app.exit_status = 1;

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
  gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(app.tree_view),
    -1, "Name", cell, "text", 0, NULL );

  gtk_widget_show_all( window );
  gtk_main();
  
  return app.exit_status;
}
