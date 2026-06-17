# lzmpo - LZ77 multiple prefixes optimal compressor

lzmpo is an experimental compressor with modified LZ77 frontend and entropy encoded
(via Turbo-Range-Coder) backend. It is designed to give good ratios on big files
(like enwik9) in relatively not-very-big amount of time. However, the drawbacks are
worse ratio on smaller files and enormous memory usage (measures in tens of gigabytes
on enwik9).

## LZ77 modifications

lzmpo implements several modifications to the standard LZ77 algorithm.

### Hasheschains

Hashchains are used to find the matches. This is the primary reason why so much
memory is used. For each prefix length (in the --pl parameter) a hashchain is
constructed and stored in RAM. This part along uses 4\*N bytes per hashchain where N
is the file size. Additionally, a hashtable is required. Its size depends on the -b
parameter: 0 makes it use \_\_gp\_hash\_table (optimized hashtable) and any other value
up to 32 creates an 4-bytes-per-value array of length 2^value. Ideally, -b0 should
be used for compressing smaller files, while -b32 is best for compressing huge files.

### Optimal parser

Instead of taking the best match, lzmpo does dynamic programming, which allows it to
pick the globally optimal path to cover the entire file with matches. Cost of a match
is estimated by a function that is somewhat close to the backend representation's price of a match. It works by calculating how many bits a match would take based on its lengths, distance, and whether it has the same distance as any of the last 3 matches. The distance cache costs 12 \* N bytes.
Matches cannot be longer than 256 bytes, so the dp calculates the closest match of size
X for each X=5...256, and then updates its states using that match. Lower bound 5 is
derived from empirical data, as setting it to 4 actually decreases the compression
ratio.
DP is done in parallel: -k defines the number of divisions of the file, and each
division has its own dp. Then those segments are merged together into a single block,
which only results in about divisions \* 256 bytes that could have been covered by
matches better.

### Max search depth

The number of jumps along the hashchain when looking for a match is limited (--mm
parameter).

### Multiple prefix lengths

The issue with **standard** LZ77 is that it calcualtes hashes of 3-byte long
substrings of the data. This makes good matches be very distant, and they are often
not discovered / it takes a really long time to find them. To fix this, lzmpo
calculates hashes of X-byte long substrings from the list of X1,X2,X3,..,XN (--pl
parameter). When looking for a match, the parser first checks the hashchain for XN.
This makes it possible to reach long matches in relatively short time. Next, other
lengths are checked in decreasing order. For Xi, the parser will try to find matches
of length 5...X(i+1)-1, as longer matches would have already been discovered during
the previous iteration.

### Threads
Almost all compression is done in multiple threads (except when calculating
hashchains and when doing entropy encoding. Number of threads is provided in the -k
parameter.

## Backend
Tokens are split into several streams:
- Controls (literal, distance0 in cache, distance1 in cache, distance2 in cache,
  match) (1 byte)
- Literals (1 byte)
- Lengths (1 byte)
- Distance ctx (1 byte)
- Distance extra bits (raw bitstream)

Distances are split (I call it binning) into groups, with lower groups allowing
smaller distances to be represented by fewer bits. Each group has: context (1-byte
index of the group), base and extra distance bits of distance - base.
Turbo-Range-Coder is used for this with all streams but extra bits being encoded via
rcmrrssdec using dual-predictor with parameters 4 and 7.

# Results
My personal goal was beating zstd -22 on enwik9, and so the results are as follows:

