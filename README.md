# UnityMediaStreaming
A package for live streaming broadcasting and receiving for Unity

How to start:

* Compile the visual studio project FFMPEGBroadcastDLL at /Native directory. You'll need this in order to 
* At Assets/UnityMediaStreaming/Scenes there are demo solution for you to explore.

You'll need a local rtmp server(or use Tools/nginx-server-gryphon, see below) running in order to run the examples.

For convinient, /Tools directory contains the following:
* ffmpeg - A tool to play, create edit and transmit media. See the scripts with demos
* nginx-server-gryphon - A nginx server with rtmp module, configured for the examples. If you want to run the examples but you don't have an rtmp server, cd to Tools/nginx-server-gryphon and run "RunNginx.bat".

This project is experimental yet, but you can use it right now in your responsibility, and very welcome to share results.

Currently it is supported only at Unity engine and only on Windows.

This is project is licensed under the MIT License. for more information, see LICENSE file.
