mkdir temp
mkdir logs

mkdir root\dash
mkdir root\hls

nginx -s stop
taskkill /IM nginx.exe /F

nginx -p "%CD%"