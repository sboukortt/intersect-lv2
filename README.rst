===============
 intersect-lv2
===============

.. default-role:: math

.. contents::

What is this?
==============

Intersect is an LV2_ plugin which, given a stereo audio stream, “expands” it to
three channels. Everything that is present in both input channels will be in the
center channel of the output, and what is specific to each channel will be in
the corresponding output channel.

.. _LV2: http://lv2plug.in/

This can be useful, for example, to rediscover some of your favorite music by
hearing things that you had never noticed before. (With that said, note that it
does not necessarily work equally well on all songs, depending on how they were
mixed.)

How to use it?
===============

Prerequisites
--------------

Intersect has been developed on Linux, and tested on Linux and Windows. It would
certainly be possible to port it to macOS, but no effort has been made toward
that so far.

Being an LV2 plugin, Intersect also requires an LV2 host. Examples include
LV2proc_ (recommended on Linux) and the famous Audacity_ editor.

.. _LV2proc: http://naspro.sourceforge.net/applications.html#lv2proc
.. _Audacity: http://www.audacityteam.org/

Getting a working copy
-----------------------

On Windows
~~~~~~~~~~~

The simplest option on Windows is to download the binaries on `the release
page`_. You should pick the ``win32`` or ``win64`` archive depending on whether
your LV2 host is 32-bit (which Audacity currently is) or 64-bit.

.. _the release page: https://github.com/sboukortt/intersect-lv2/releases

Assuming that your Windows installation lives on drive ``C:``, you can then drag
and drop the ``intersect.lv2`` folder from the archive to either
``C:\Users\<user name>\AppData\Roaming\LV2`` to install it for your user only,
or ``C:\Program Files\Common Files\LV2`` to install it globally. Replace
``Program Files`` with ``Program Files (x86)`` if you have a 64-bit edition of
Windows and a 32-bit LV2 host.

On Linux
~~~~~~~~~

No binaries are provided for Linux. Therefore, you should compile the plugin on
your own machine.

To that effect, you need:

- `pkg-config <https://www.freedesktop.org/wiki/Software/pkg-config/>`_
- `tup <http://gittup.org/tup/>`_
- a C compiler (the build files use `Clang <https://clang.llvm.org/>`_ by
  default but they should be easy to change if you prefer to use
  `GCC <https://gcc.gnu.org/>`_)
- development files for `FFTW <http://fftw.org/>`_ and
  `LV2 <http://lv2plug.in/>`_

You can then run the following from Intersect’s source tree:

.. code:: console

	$ tup variant config/release.config
	$ tup

Finally, you can then copy the ``build-release/intersect.lv2`` folder to either
``$HOME/.lv2/`` or ``/usr/lib/lv2/``.

Actual usage
-------------

With LV2Proc
~~~~~~~~~~~~~

Basic usage looks as follows:

.. code:: console

	$ lv2proc --normalize -i input.flac -o output.flac 'https://sami.boukortt.com/plugins/intersect#Upmix'

``--normalize`` is optional and results in lv2proc (you guessed it) normalizing
the output to -0dB. It can prevent clipping if the input file is too loud.

You can also specify different values for the parameters using ``-c``, for
example: ``-c fft_window_size:8192``. (See the section on parameters_ for more
details.)

Script
:::::::

For your convenience, a script is provided at the root of the source tree. It is
a very simple script (as you will see if you look at its source code) that
allows you (requires you, actually) to leave out the URI of the plugin. Assuming
that you put the script on your ``PATH``, the example above becomes:

.. code:: console

	$ intersect --normalize -i input.flac -o output.flac

With Audacity
~~~~~~~~~~~~~~

Unfortunately, at the moment, a bug in Audacity prevents effects from turning a
2-channel track into three channels. Consequently, two additional effects are
provided, each producing part of Intersect’s full output:

- “Channel Intersection” produces the center channel;
- “Channel Symmetric Difference” produces the left and right channels.

After installing Intersect, you might need to enable the effects in Audacity
using “Effects” → “Add / Remove Plug-ins…”. Those effects will then appear at
the bottom of the “Effects” menu, under “Plug-ins 1 to 15” (or different
numbers if you already have a lot of plugins).

:Note:
	in case that bug is ever fixed, the full Intersect effect appears as
	“2.0 -> 3.0 Upmix”.

Parameters
-----------

A few parameters can be set if desired, altough they should have sensible
defaults.

FFT Window Size
~~~~~~~~~~~~~~~~

:Default value: 16384
:LV2 port name: ``fft_window_size``

Number of samples on which to perform a Fourier transform at a time. Higher
values increase the frequency resolution, at the expense of temporal resolution
(but you can increase the `overlap factor`_ to make up for it).

Overlap Factor
~~~~~~~~~~~~~~~

:Default value: 512
:LV2 port name: ``overlap_factor``

Intersect performs FFTs over overlapping windows. For example, with an overlap
factor of 2, the following transforms will be computed:

.. code::

	Input:                [----------------------------]

	Transforms:
	            [--------]
	                 [--------]
	                      [--------]
	                           [--------]
	                                [--------]
	                                     [--------]
	                                          [--------]
	                                               [--------]
	                                                    [--------]

That is, at each step, the beginning position of the transform is increased by
`\frac{\text{FFT window size}}{\text{overlap factor}}`, not by a full
`\text{FFT window size}`.

Thus, the overlap factor is the number of transforms that are applied to a given
sample. The corresponding output sample is computed from the average of the
result of processing each of those transforms.

Increasing this number improves temporal resolution but also increases the
processing time required.
