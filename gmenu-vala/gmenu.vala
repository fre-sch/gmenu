enum Keysym {
  Up = 65362,
  Down = 65364,
  Left = 65361,
  Right = 65363,
  Enter = 65293,
  Escape = 65307,
  Tab = 65289,
  LShift = 65505,
  RShift = 65506,
}

class GMenu : Object {
  private uint timeout_id;
  private ulong on_entry_changed_id;
  private Gtk.Entry entry;
  private Gtk.TreeView tree_view;
  private Gtk.TreeModelFilter filter;
  
  private void fill_treemodel( Gtk.ListStore m ) {
    string line;
    while ( (line = stdin.read_line() ) != null ) {
      Gtk.TreeIter i;
      m.append( out i );
      m.set( i, 0, line, -1 );
    }
  }
  private Gtk.TreeModel new_treemodel() {
    var tm = new Gtk.ListStore( 1, typeof(string) );
    fill_treemodel( tm );
    
    filter = new Gtk.TreeModelFilter( tm, null );
    filter.set_visible_func( filter_func );
    
    return filter;
  }
  private Gtk.ScrolledWindow new_treeview() {
    tree_view = new Gtk.TreeView.with_model( new_treemodel() );
    tree_view.unset_flags(Gtk.WidgetFlags.CAN_FOCUS);
    tree_view.enable_search = false;
    tree_view.headers_visible = false;
    tree_view.get_selection().type = Gtk.SelectionMode.SINGLE;
    tree_view.get_selection().changed.connect( on_treeview_selection_changed );
    
    var cr = new Gtk.CellRendererText();
    cr.ellipsize = Pango.EllipsizeMode.START;
    tree_view.insert_column_with_data_func(
      -1, "Data", cr, cell_data_func );
    
    var sw = new Gtk.ScrolledWindow( null, null );
    sw.set_policy( Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC );
    sw.set_shadow_type( Gtk.ShadowType.IN );
    sw.add( tree_view );
    
    return sw;
  }
  private void on_treeview_selection_changed() {
    Gtk.TreeModel m;
    Gtk.TreeIter i;
    var sel = tree_view.get_selection();
    var selected = sel.get_selected( out m, out i );
    if ( selected ) {
      string val;
      m.get( i, 0, out val, -1 );
      SignalHandler.block( entry, on_entry_changed_id );
      entry.text = val;
      SignalHandler.unblock( entry, on_entry_changed_id );
      entry.select_region(0, -1);
    }
  }
  private void on_entry_changed() {
    if ( timeout_id != 0 )
      Source.remove( timeout_id );
    timeout_id = Timeout.add( 300, on_input_timeout );
  }
  private bool on_input_timeout() {
    filter.refilter();
    timeout_id = 0;
    return false;
  } 
  private bool filter_func( Gtk.TreeModel m, Gtk.TreeIter i ) {
    string haystack;
    var needle = entry.text;
    if ( needle.len() == 0 ) {
      return true;
    }
    m.get( i, 0, out haystack, -1 );
    return haystack.casefold().contains( needle.casefold() );
  }
  private void cell_data_func( Gtk.TreeViewColumn col, Gtk.CellRenderer cr, Gtk.TreeModel mdl, Gtk.TreeIter itr ) {
    weak string search = entry.text;
    var search_len = search.len();
    string item;
    mdl.get( itr, 0, out item );
    
    if ( search_len == 0 ) {
      (cr as Gtk.CellRendererText).markup = item;
    }
    else {
      string markup, lead, body, tail;
      string itemi = item.casefold();
      string searchi = search.casefold();
      if ( itemi.contains( searchi ) ) {
        weak string? leadi = itemi.str( searchi );
        var ofs = itemi.pointer_to_offset( leadi );
        if ( ofs > 0 ) {
          lead = item[ 0 : ofs ];
          markup = "".concat( lead );
        }
//        middle = item[ ofs : search_len ];
//        markup = markup.concat( "<b>", middle, "</b>" );
//        if ( ofs + search_len < item.len() ) {
//          tail = item[ ofs + search_len : -1 ];
//          markup = markup.concat( tail );
//        }
      }
      else {
        markup = item;
      }

      (cr as Gtk.CellRendererText).markup = markup;
    }
  }
  private void select_last() {
    Gtk.TreeModel mdl;
    Gtk.TreeIter itr;
    var sel = tree_view.get_selection();
    sel.get_selected( out mdl, out itr );
    var n = mdl.iter_n_children( null );
    if ( n > 0 ) {
      if ( mdl.iter_nth_child( out itr, null, n - 1 ) ) {
        tree_view.scroll_to_cell( mdl.get_path( itr ), null, false, 0.0f, 0.0f );
        sel.select_iter( itr );
      }
    }
  }
  private void select_next() {
    Gtk.TreeModel mdl;
    Gtk.TreeIter itr;
    var sel = tree_view.get_selection();
    bool itr_ok = false;
    if ( sel.get_selected( out mdl, out itr ) ) {
      itr_ok = mdl.iter_next( ref itr );
    }

    if ( !itr_ok ) {
      itr_ok = mdl.get_iter_first( out itr );
    }

    if ( itr_ok ) {
      tree_view.scroll_to_cell( mdl.get_path( itr ), null, false, 0.0f, 0.0f );
      sel.select_iter( itr );
    }
  }
  private void select_previous() {
    Gtk.TreeModel mdl;
    Gtk.TreeIter itr;
    var sel = tree_view.get_selection();
    if ( sel.get_selected( out mdl, out itr ) ) {
      var p = mdl.get_path( itr );
      if ( p.prev() ) {
        tree_view.scroll_to_cell( p, null, false, 0.0f, 0.0f );
        sel.select_path( p );
      }
      else {
        select_last();
      }
    }
    else {
      select_last();
    }
  }
  private bool on_entry_key_press_event( Gdk.EventKey e ) {
    if ( e.keyval == Keysym.Escape ) {
      Gtk.main_quit();
      return true;
    }
    else if ( e.keyval == Keysym.Tab || e.keyval == Keysym.Down ) {
      select_next();
      return true;
    }
    else if ( e.keyval == Keysym.Up ) {
      select_previous();
      return true;
    }
    return false;
  }
  private Gtk.Window new_window() {
    entry = new Gtk.Entry();
    on_entry_changed_id = entry.changed.connect( on_entry_changed );
    entry.key_press_event.connect( on_entry_key_press_event );
    
    var box = new Gtk.VBox( false, 2 );
    box.pack_start( entry, false, false, 0);
    box.pack_start( new_treeview(), true, true, 0);
    
    var win = new Gtk.Window( Gtk.WindowType.TOPLEVEL );
    win.title = "gmenu";
    win.position = Gtk.WindowPosition.CENTER;
    win.destroy.connect( Gtk.main_quit );
    win.set_default_size( 240, 320 );
    win.type_hint = Gdk.WindowTypeHint.DIALOG;
    win.border_width = 2;
    win.add( box );
    
    return win;
  }

  static int main( string[] args ) {
    Gtk.init( ref args );
    new GMenu().new_window().show_all();
    Gtk.main();
    return 0;
  }
}

