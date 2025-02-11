=========
hpcviewer
=========
-----------------------------------------------------------------
interactively presents program performance in a top-down fashion.
-----------------------------------------------------------------

:manual_section: 1
:manual_group: The HPCToolkit Performance Tools
:date: @DATE@
:version: @PROJECT_VERSION@
:author:
  Rice University's HPCToolkit Research Group:
  <`<https://hpctoolkit.org/>`_>
:contact: <`<hpctoolkit-forum@rice.edu>`_>
:copyright:
  Copyright © 2002-2023 Rice University.
  HPCToolkit is distributed under @LICENSE_RST@.


SYNOPSIS
========

Command line usage:
  ``hpcviewer`` [*options*]... [*hpctoolkit_database*]

GUI usage:
  Launch hpcviewer and open the experiment database [*hpctoolkit_database*].

DESCRIPTION
===========

The Java-based hpcviewer interactively presents program-performance experiment databases in a top-down fashion.
Since experiment databases are self-contained, they may be relocated from a cluster for visualization on a laptop or workstation.

ARGUMENTS
=========

*hpctoolkit_database*
   An HPCToolkit experiment database produced by hpcprof/hpcprof-mpi.

OPTIONS
-------

-h, --help  Print a help message.

``-jh``, ``--java-heap`` *size*
   Set the JVM maximum heap size for this execution of hpcviewer.
   The value of size must be in megabytes (M) or gigabytes (G).
   For example, one can specify a size of 3 gigabytes as either 3076M or 3G.

DATA REPRESENTATION
===================

The database generated by hpcprof/hpcprof-mpi consists of 4 dimensions: *execution context* (also called *execution profile*), *time*, *tree node context*, and *metric*.
The term *execution context* includes any logical threads (such as OpenMP, pthread and C++ threads), and also MPI processes and GPU streams.
The *time* dimension represents the timeline of the program's execution, and *tree node context* depicts a top-down path in a calling-context tree.
This time dimension is only available if the application is profiled with traces enabled (hpcrun -t option).
Finally, the *metric* dimension constitutes program measurements performed by hpcrun such as cycles, number of instructions, stall percentages and also some derived metrics such as ratio of idleness.

The following table summarizes different views supported by hpcviewer:

+----------------------+----------------------+----------------------+
| View                 | Dimension            | Note                 |
+======================+======================+======================+
| Profile view - Table | Tree node context x  | display the tree and |
|                      | Metrics              | its associated       |
|                      |                      | metrics              |
+----------------------+----------------------+----------------------+
| Profile view -       | Tree node context x  | display the tree and |
| Thread               | Metrics              | its metrics for a    |
|                      |                      | set of execution     |
|                      |                      | contexts             |
+----------------------+----------------------+----------------------+
| Profile view - Graph | Execution contexts x | display a metric of  |
|                      | Metrics              | a specific tree node |
|                      |                      | for all execution    |
|                      |                      | contexts             |
+----------------------+----------------------+----------------------+
| Trace view - Main    | Execution contexts x | display execution    |
|                      | Time                 | context behavior     |
|                      |                      | over time            |
+----------------------+----------------------+----------------------+
| Trace view - Depth   | Tree node context x  | display call stacks  |
|                      | Time                 | over time of an      |
|                      |                      | execution context    |
+----------------------+----------------------+----------------------+

PROFILE VIEW
============

VIEWS
-----

hpcviewer supports four principal views of an application's performance data.
Each view reports both inclusive costs (including callees) and exclusive costs (excluding callees).

Top-down view
  A top-down view depicting the dynamic calling contexts (call paths) in which costs were incurred.
  With this view one can explore the performance of an application in a top-down manner to understand the costs incurred by calls to a procedure in a particular calling context.

Bottom-up view
  This bottom-up view enables one to look upward along call paths.
  It apportions a procedure's costs to its caller and, more generally, its calling contexts.
  This view is particularly useful for understanding the performance of software components or procedures that are used in more than one context.

Flat view
  This view organizes performance data according to the static structure of an application.
  All costs incurred in *any* calling context by a procedure are aggregated together in the flat view.

Thread view
  This view displays the metric values of *execution contexts*.
  An execution context is an abstraction of a measure of code execution such as threads and processes.
  To activate the view, select a thread or a set of execution contexts by clicking the thread-view button from the Top-down view.
  On the execution context selection window, select the checkbox of the execution contexts of interest.
  To narrow the list, specify the name on the filter part of the window.
  Once the execution contexts have been selected, click **OK**, and the Thread view will be activated.
  The tree of the view is the same as the tree from the Top-down view, with the metrics only from the selected execution contexts.
  If there is more than one selected thread, the metrics are the sum of the metric values.

  Currently contexts for GPU streams are not shown in the thread view; metrics for a GPU operation are associated with the calling context in the thread that initiated the GPU operation.

PANES
-----

The browser window is split into two panes:

Source pane
  The source pane appears at the top of the browser window and displays source code associated with the current selection in the navigation pane below.
  Making a selection in the navigation pane causes the source pane to display the selection's corresponding source file and highlight the source line.

Navigation pane
  The navigation pane appears at the bottom left of the browser window and displays an outline (tree structure) organizing the performance measurements under investigation.
  Each item in the outline denote a structure in the source code such as a load module, source file, procedure, procedure activation, loop, single line of code, or code fragment inlined from elsewhere.
  Outline items can be selected and their children folded and unfolded.

Which items appear in the outline depend on which view is displayed:

- In the Top-down view, displayed items are procedure activations, loops, source lines, and inlined code.
  Most items link to a single location in the source code, but a procedure activation item links to two: the call site where the procedure was invoked and the procedure body executed in response.

- In the Bottom-up view, displayed items are always procedure activations.
  Unlike the Top-down view, where a call site is paired with its called procedure, in this view a call site is paired with its calling procedure, attributing costs for a called procedure among all its call sites (and therefore callers).

- In the flat view, displayed items are source files, call sites, loops, and source lines.
  Call sites are rendered in the same way as procedure activations.

The header above the navigation pane contains buttons for adjusting the displayed view:

Up arrow
 *Zoom in* to show only information for the selected line and its descendants.

Down arrow
  *Zoom out* to reverse a previous zoom-in operation.

Hot path
  Toggle hot path mode, which automatically unfolds subitems along the *hot path* for the currently selected metric: those subitems encountered by starting at the selected item and repeatedly descending to the child item with largest cost for the metric.
  This is an easy way to find performance bottlenecks for that metric.

Derived metric
  Define a new metric in terms of existing metrics by entering a spreadsheet-style formula.

Filter metrics
  Show the metric property view which allows to show or hide specified metrics of the current table.
  One can also edit the name of a metric column or even edit the formula of a derived metric.

Resize metric columns
  Resizing metric columns based on either the width of the data or the width of both the data and the column's label.

CSV export
  Write data from the current table to a file in standard CSV (Comma Separated Values) format.

Bigger text
  Increase the size of displayed text.

Smaller text
  Decrease the size of displayed text.

Showing graph of metric values
  Showing the graph (plot, sorted plot or histogram) of metric values of the selected node in CCT for all processes or threads.
  Hovering the mouse over the dot will show the information of the rank or thread and its metric value.
  Right clicking on the graph will show menus to adjust the axis, zoom-in/out, display the setting and display the thread view of the cursor-pointed dot graph.

  .. note::
    Currently contexts for GPU streams are not shown in the x-axis.
    The metrics for a GPU operation are associated with the calling context in the thread that initiated the GPU operation.

Show the metrics of a set of execution contexts
  Shows the CCT and the metrics of a selected execution contexts (ranks and/or threads).
  If the set contains more than one execution contexts, then the value is the sum of the value of the selected execution contexts.

Flatten *(icon of a slashed tree node)*
  *Flatten* the navigation pane outline, i.e. replace each top-level item by its child subitem (available in flat view only).
  If an item has no children it remains in the outline.
  Flattening may be performed repeatedly, each step hiding another level of the outline.
  This is useful for relaxing the strict hierarchical view so that peers at the same level in the tree can be viewed and ranked together.
  For instance, this can be used to hide procedures in the flat view so that outermost loops can be ranked and compared.

Unflatten
  Undo one previous flatten operation (flat view only).

Metric pane
  The metric pane appears to the right of the navigation pane at the bottom of the window and displays one or more columns of performance data, one metric per column.
  Each row displays measured metric values for the source structure denoted by the outline item to its left.
  A metric may be selected by clicking on its column header, causing outline items at each level of the hierarchy to be sorted by their values for that metric.

PLOT GRAPHS
-----------

hpcviewer can display graphs of thread-level metric values which is useful for quickly assessing load imbalance across processes and threads.

To create a graph, choose the top-down view and select an item in the navigation pane, then click the graph button above the navigation pane.
A list of graphable metrics appears at the bottom of the context menu, each with a sub-menu showing the three graph styles that hpcviewer can make.
The *Plot* graph displays metrics ordered by execution context.
The *Sorted plot* graph displays metrics sorted by value; and the *Histogram* graph displays a barchart of metric value distributions.

Notes:

- the plot graph's execution context have the following notation::

    <process_id> . <thread_id>

  Hence, if the ranks are 0.0, 0.1, . . . 31.0, 31.1 it means MPI process 0 has two threads: thread 0 and thread 1 (similarly with MPI process 31).

- In the Profile view, operations on any GPU stream execution context are reported by the thread that offloaded them onto the GPU stream.

- Currently, it is only possible to generate scatter plots for metrics directly collected by hpcrun, which excludes derived metrics created by hpcviewer.

TRACE VIEW
==========

The view interactively presents program traces in a top-down fashion.
It comprises of three different parts.

Main view (left, top)
  This is the primary view.
  This view, which is similar to a conventional process/time (or space/time) view, shows time on the horizontal axis and process (or thread) rank on the vertical axis; time moves from left to right.
  Compared to typical process/time views, there is one key difference.
  To show call path hierarchy, the view is actually a user-controllable slice of the process/time/call-path space.
  Given a call path depth, the view shows the color of the currently active procedure at a given time and process rank.
  (If the requested depth is deeper than a particular call path, then the viewer simply displays the deepest procedure frame and, space permitting, overlays an annotation indicating the fact that this frame represents a shallower depth.)
  hpcviewer assigns colors to procedures based on (static) source code procedures.
  Although the color assignment is currently random, it is consistent across the different views.
  Thus, the same color within the Trace and Depth Views refers to the same procedure.
  The Trace View has a white crosshair that represents a selected point in time and process space.
  For this selected point, the Call Path View shows the corresponding call path.
  The Depth View shows the selected process.

Depth view (left, bottom)
  This is a call-path/time view for the process rank selected by the Trace view's crosshair.
  Given a process rank, the view shows for each virtual time along the horizontal axis a stylized call path along the vertical axis, where 'main' is at the top and leaves (samples) are at the bottom.
  In other words, this view shows for the whole time range, in qualitative fashion, what the Call Path View shows for a selected point.
  The horizontal time axis is exactly aligned with the Trace View's time axis; and the colors are consistent across both views.
  This view has its own crosshair that corresponds to the currently selected time and call path depth.

Summary view (same location as depth view on the left-bottom part)
  The view shows for the whole time range displayed, the proportion of each subroutine in a certain time.
  Similar to Depth view, the time range in Summary reflects to the time range in the Trace view.

Call stack view (right, top)
  This view shows two things: (1) the current call path depth that defines the hierarchical slice shown in the Trace View; and (2) the actual call path for the point selected by the Trace View's crosshair.
  (To easily coordinate the call path depth value with the call path, the Call Path View currently suppresses details such as loop structure and call sites; we may use indentation or other techniques to display this in the future.)

Statistics view (tab in top, right pane)
  The view shows a list of procedures and the estimated execution percentage for each for the time interval currently shown in the Trace view.

GPU Blame view (tab in top, right pane)
  This view shows the list of procedures that cause GPU idleness displayed in the trace view.

Mini map view (right, bottom)
  The Mini Map shows, relative to the process/time dimensions, the portion of the execution shown by the Trace View.
  The Mini Map enables one to zoom and to move from one close-up to another quickly.

Note:

- GPUs are very fast, hence the time interval during which a GPU operation is active may be very short.
  A problem for users is that it may be hard to locate short GPU operations that are separated by long intervals of idleness in the trace.
  Such operations will often be invisible because when hpcviewer renders a pixel in a trace, it will not show a GPU operation unless the time point at the left edge of the pixel's associated time interval falls within the time interval of the GPU operation.
  To force hpcviewer to render a GPU operation if any GPU operation is active within the time interval associated with a pixel, one can enable *Expose GPU traces* by clicking the menu **File** - **Preferences** and click the **Traces** page, then check the *Expose GPU traces* option.

  .. WARNING::
    Enabling this option causes trace statistics to be unreliable because GPU activity will be overrepresented.

MAIN VIEW
---------

Main view is divided into two parts: the top part which contains *action pane* and the *information pane*, and the main view which displays the traces.

The buttons in the action pane are the following:

Home
  Reset the view configuration into the original view, i.e., viewing traces for all times and processes.

Horiontal zoom in / out
  Zoom in/out the time dimension of the traces.

Vertical zoom in / out
  Zoom in/out the process dimension of the traces.

Navigation buttons
  Navigate the trace view to the left, right, up and bottom, respectively.
  It is also possible to navigate with the arrow keys in the keyboard.
  Since Trace view does not support scroll bars, the only way to navigate is through navigation buttons (or arrow keys).

Undo
  Cancel the action of zoom or navigation and returning back to the previous view configuration.

Redo
  Redo of previously undo change of view configuration.

Save/Load a view configuration
  Save/load a saved view configuration.
  A view configuration file contains the information of the current dimension of time and process, the depth and the position of the crosshair.
  It is recommended to store the view configuration file in the same directory as the database to ensure that the view configuration file matches well with the database since the file does not store which database it is associated with.
  Although it is possible to open a view configuration file which is associated from different database, it is highly not recommended since each database has different time/process dimensions and depth.

The information pane contains some information concerning the range status of the current displayed data.

Time Range
  The information of current time-range (horizontal) dimension.

Cross Hair
  The information of current crosshair position in time and execution-context dimensions.

DEPTH VIEW
----------

Depth view shows all the call path for a certain time range [t_1,t_2]= {t \| t_1 <= t <= t_2} in a specified process rank p.
The content of Depth view is always consistent with the position of the cross-hair in Trace view.
For instance once the user clicks in process p and time t, while the current depth of call path is d, then the Depth view's content is updated to display all the call path of process p and shows its cross-hair on the time t and the call path depth d.

On the other hand, any user action such as cross-hair and time range selection in Depth view will update the content within Trace view.
Similarly, the selection of new call path depth in Call view invokes a new position in Depth view.

In Depth view a user can specify a new cross-hair time and a new time range.

Specifying a new cross-hair time
  Selecting a new cross-hair time t can be performed by clicking a pixel within Depth view.
  This will update the cross-hair in Trace view and the call path in Call view.

Selecting a new time range
  Selecting a new time range [t_m,t_n]= {t \| t_m <= t <= t_n} is performed by first clicking the position of t_m and drag the cursor to the position of t_n.
  A new content in Depth view and Trace view is then updated.
  Note that this action will not update the call path in Call view since it does not change the position of the cross-hair.

SUMMARY PANE
------------

Summary view presents the proportion of number of calls of time t across the current displayed rank of process p.
Similar to Depth view, the time range in Summary view is always consistent with the time range in Trace view.
One can also select a new time range in this view.

CALL STACK PANE
---------------

This view lists the call path of process *p* and time *t* specified in Trace view and Depth view.
This view can show a call path from depth 0 to the maximum depth, and the current depth is shown in the depth editor (located on the top part of the view).

In this view, the user can select the depth dimension by either typing the depth in the depth editor or selecting a procedure in the table of call path.

STATISTICS PANE
---------------

The view shows a list of procedures and the estimated execution percentage for each for the time interval currently shown in the Trace view.
Whenever the user changes the time interval displayed in the Trace view, the statistics view will update its list of procedures and their execution percentages to reflect the current interval.
Similarly, a change in the selected call path depth will also update the contents of the statistics view.

MINI MAP PANE
-------------

The Mini view shows, relative to the process/time dimensions, the portion of the execution shown by the Trace view.
In Mini view, the user can select a new process/time (p_a,t_a),(p_b,t_b) dimensions by clicking the first process/time position (p_a,t_a) and then drag the cursor to the second position (p_b,t_b).
The user can also moving the current selected region to another region by clicking the white rectangle and drag it to the new place.

MENUS
=====

hpcviewer provides five main menus:

FILE
----

This menu includes several menu items for controlling basic viewer
operations.

New window
  Open a new hpcviewer window that is independent from the existing one.

Switch database
  Load a performance database into the current hpcviewer window and close all opened databases (if any).

Open database
  Load a performance database into the current hpcviewer window.
  Currently hpcviewer restricts maximum of five database open at a time.
  To display more, one can either closing an existing open database, or opening a new hpcviewer window.

Close database
  Unloading a performance database.

Merge database
  Merging two database that are currently in the viewer.
  If hpcviewer has more than two open database, then one needs to choose which database to be merged.
  Currently hpcviewer does not support storing a merged database into a file.

  Merge top-down tree
    Merging the top-down trees of the two opened database.

  Merge flat tree
    Merging the flat trees of the two opened database.

Preferences...
  Display the settings dialog box which consists of three sections:

  Appearance
    Change the fonts for tree and metric columns and source viewer.

  Traces
    Specify settings for Trace view such as the number of working threads to be used and the tooltip's delay.

  Debug
    Enable/disable debug mode.

Exit
  Quit the hpcviewer application.

FILTER
------

This menu is to allow users to filter certain nodes in the Profile view or filter certain profiles in the Trace view.

Filter execution contexts *(Trace view mode only)*
  Open a window for selecting which ranks or threads or GPUs should be displayed.

Filter CCT nodes
  Open a filter property window which lists a set of filters and its properties.
  hpcviewer allows users to define multiple filters, and each filter is associated with a type and a glob pattern (A glob pattern specifies which name to be removed by using wildcard characters such as \*, ? and +).
  There are three types of filter: "self only" to omit matched nodes, "descendants only" to exclude only the subtree of the matched nodes, and "self and descendants" to remove matched nodes and its descendants.
  An hpcviewer filter set applies globally; namely, it applies to all open databases in all windows.
  An hpcviewer filter set is saved on disk and any active filters will affect future hpcviewer sessions as well.

VIEW
----

This menu is only visible if at least one database is loaded.
All actions in this menu are intended primarily for tool developer use.
By default, the menu is hidden.
Once a database is loaded, the menu is then visible.

Show metric
  Show the metric property view which allows to show or hide specified metrics of the current table.
  One can also edit the name of a metric column or even edit the formula of a derived metric.

Split window
  Enabled if there are two databases open.
  This menu allows to split vertically two databases into two panes to easily compare them.

Color map *Trace view only*
  to open a window which shows customized mapping between a procedure pattern and a color.
  hpcviewer allows users to customize assignment of a pattern of procedure names with a specific color.

Debug *(if the debug mode is enabled)*
  A special set of menus for advanced users when the debug mode is enabled.
  The menu is useful to debug hpcviewer. The menu consists of:

  Show database raw's XML
    Enable one to request display of raw XML representation for performance data.

HELP
----

This menu displays information about the viewer.

About
  Displays brief information about the viewer, including JVM and Eclipse variables, and error log files.

SEE ALSO
========

|hpctoolkit(1)|

.. |hpctoolkit(1)| replace:: **hpctoolkit**\(1)
