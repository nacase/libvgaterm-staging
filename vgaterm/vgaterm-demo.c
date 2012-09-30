/*
 * Compile with:
 *     CFILES="vgaterm-demo.c vgaterm.c vgatext.c vgafont.c vgapalette.c emulation.c scrollbuf.c cbuf.c"
 *     gcc $CFILES -o vgaterm-demo `pkg-config --cflags --libs gtk+-2.0 gthread-2.0`
 */

#include <gtk/gtk.h>

#include "vgaterm.h"

/* This is a callback function. The data arguments are ignored
 * in this example. More on callbacks below. */
static void hello( GtkWidget *widget,
                   gpointer   data )
{
    g_print ("Hello World\n");
}

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
    /* If you return FALSE in the "delete-event" signal handler,
     * GTK will emit the "destroy" signal. Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit?'
     * type dialogs. */

    g_print ("delete event occurred\n");

    /* Change TRUE to FALSE and the main window will be destroyed with
     * a "delete-event". */

    return TRUE;
}

/* Another callback */
static void destroy( GtkWidget *widget,
                     gpointer   data )
{
    gtk_main_quit ();
}

int main( int   argc,
          char *argv[] )
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget *window;
    VGATerm *term;

    /* Required threading stuff */
    if (!g_thread_supported())
        g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();

    
    /* This is called in all GTK applications. Arguments are parsed
     * from the command line and are returned to the application. */
    gtk_init (&argc, &argv);
    
    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    term = VGA_TERM(vga_term_new());
    
    /* When the window is given the "delete-event" signal (this is given
     * by the window manager, usually by the "close" option, or on the
     * titlebar), we ask it to call the delete_event () function
     * as defined above. The data passed to the callback
     * function is NULL and is ignored in the callback function. */
    g_signal_connect (window, "delete-event",
		      G_CALLBACK (delete_event), NULL);
    
    /* Here we connect the "destroy" event to a signal handler.  
     * This event occurs when we call gtk_widget_destroy() on the window,
     * or if we return FALSE in the "delete-event" callback. */
    g_signal_connect (window, "destroy",
		      G_CALLBACK (destroy), NULL);
    
    /* Sets the border width of the window. */
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    
    /* This packs the button into the window (a gtk container). */
    gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET(term));

    /* Demo text */
    vga_term_clrscr(term);
    vga_term_set_fg(term, 0x9);
    vga_term_writeln(term, "Hello world");
    vga_term_set_fg(term, 0x7);
    vga_term_writeln(term, "End of demo");
    
    /* The final step is to display this newly created widget. */
    gtk_widget_show (GTK_WIDGET(term));
    
    /* and the window */
    gtk_widget_show (window);
    
    /* All GTK applications must have a gtk_main(). Control ends here
     * and waits for an event to occur (like a key press or
     * mouse event). */
    gtk_main ();

    gdk_threads_leave();

    
    return 0;
}
