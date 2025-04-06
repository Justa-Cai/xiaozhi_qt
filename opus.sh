set -e

export PATH=`pwd`/ffmpeg:$PATH
# opusenc --raw --raw-rate 16000 --raw-chan 1 build/send.opus build/send.opus.ogg
# ffplay build/send.opus.ogg

set -x
ffmpeg -f opus -ar 16000 -ac 1 -i build/send.opus build/send.opus.ogg
ffplay build/send.opus.ogg
