set -e
set -x
ffplay -f s16le -ar 16000 -ac 1 build/send.pcm