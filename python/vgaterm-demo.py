#!/usr/bin/python
import sys
import string
import getopt
import gtk
import gobject
import vgaterm

scroll_i = 0

def key_press_event(sw, event):
    #print event.state
    #print "keyval = %d" % event.keyval
    global scroll_i
    if event.keyval == 65362:
        print "Up"
        scroll_i += 1
        print "set_scroll(%d)" % scroll_i
        sw.term.set_scroll(scroll_i)
    elif event.keyval == 65364:
        print "Down"
        scroll_i -= 1
        if (scroll_i < 0):
            scroll_i = 0
        sw.term.set_scroll(scroll_i)

def close_application(widget):
    gtk.main_quit()

if __name__ == '__main__':
    # Required thread-aware init
    gobject.threads_init()
    gtk.gdk.threads_init()
    gtk.gdk.threads_enter()

    use_scrolledwin = True
    use_textview = False

    window = gtk.Window(gtk.WINDOW_TOPLEVEL)
    window.set_resizable(True)
    window.connect("destroy", close_application)
    window.set_title("VGATerm Demo")
    window.set_border_width(0)

    vbox = gtk.VBox(False, 0)
    window.add(vbox)
    vbox.show()

    if use_scrolledwin:
        sw = gtk.ScrolledWindow()
        sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_ALWAYS)
    else:
        sw = gtk.Window()
        sw.connect("destroy", gtk.main_quit)

    sw.set_size_request(640, 400)
    vbox.pack_start(sw, True, True, 2)

    button = gtk.Button("Test")
    button.connect("clicked", close_application)
    vbox.pack_end(button, False, False, 2)
    button.show()
   
    child = None
    widget = None

    if not use_textview:
        term = vgaterm.VGATerm()

        term.clrscr()
        term.set_fg(chr(0x7));
        for x in range(50):
            term.writeln("Line %d" % x)
        term.set_fg(chr(0x9))
        term.writeln("Hello world")
        term.set_fg(chr(0x7))
        term.writeln("End of demo")
        window.term = term
        term.show()
        widget = term
    else:
        textview = gtk.TextView(buffer=None)
        textbuffer = textview.get_buffer()
        textview.show()
        widget = textview

    window.connect("key-press-event", key_press_event)

    if not use_scrolledwin:
        scrollbar = gtk.VScrollbar()

        box = gtk.HBox()
        box.pack_start(widget)
        box.pack_start(scrollbar)
        child = box
    else:
        child = widget

    sw.add(child)
    sw.show()

    window.show()
    gtk.main()
    gtk.gdk.threads_leave()
