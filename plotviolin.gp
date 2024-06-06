reset session
set encoding utf8

isPngTerm = 0
isColorbox = 0
isXtics = 0
isInfoTop = 0
isPosBottom = 0
isPosTop = 1
isYorigTop = 1
isCurve  = 0
isUseMaxSample = 0
isDebug = 1


if(isPngTerm) {
    set term 'png' enhanced truecolor size 1010,1000
}

if(GPVAL_TERM eq 'qt') {
    set term qt size 1010,1000
    isQT = 1

    corpusTitle = "Github Corpus"

    ARGC = 2
    
    corpusTitle = "Random Corpus"
    
    if(ARGC != 2) {

	    ARG1 = 'E:\git\smallz4'
        #ARG1 = 'C:\Users\gbonneau\git\smallz4'

        #ARG6 = "lz4_canterbury_corpus.txt_4096.csv"
        #ARG5 = "lz4_silicia_corpus.txt_4096.csv"    
        #ARG4 = "lz4_calgary_corpus.txt_4096.csv"
        #ARG3 = "lz4_enwik9.txt_4096.csv"
        #ARG2 = "lz4_random.txt_4096.csv"

        ARG2 = "github_2GB_b_32K_w_4K.csv"
        ARG3 = "github_2GB_b_32K.csv"
        ARG4 = "Github_64_MB_4096.csv"
        ARG5 = "Github_128_MB_4096.csv"
        ARG6 = "Github_256_MB_4096.csv"
        ARG7 = "Github_512_MB_4096.csv"
        ARG8 = "Github_1_GB_4096.csv"
        ARG9 = "Github_2_GB_4096.csv"

        #ARG4 = "lz4_Wikipedia Corpus.txt_4096.csv"
        #ARG3 = "lz4_Wikipedia Corpus.txt_2048.csv"
        #ARG2 = "lz4_Wikipedia Corpus.txt_1024.csv"
    }
    
    if(ARGC == 2) {

	   ARG1 = 'E:\git\smallz4'

       #ARG2 = "lz4_silicia_corpus.txt_4096.csv" 
       #ARG2 = "lz4_github_corpus.txt_4096_data_20_3GB.csv"
	   #ARG2 = "lz4_github_corpus.txt_4096_data_20_240MB.csv"
	   ARG2 = "github_128MB_4K.csv"
    }
    print "Termninal QT"
}
else { 
    set term 'png' enhanced truecolor size 1010,1000
    isQT = 0

    corpusTitle = "Silesia Corpus"

    if(ARGC != 2) {
        ARGC = 2
        ARG1 = 'C:\Users\gbonneau\git\smallz4'
        ARG2 = "github_2GB_b_32K.csv"
    }
    set output ARG2.sprintf(".png")
    print "Terminal PNG"
}
show terminal

array corpusFiles[ARGC-1]
array corpusNames[ARGC-1]

numARG = (9 < ARGC) ? 8 : ARGC-1

do for [i=1:numARG] {
  eval sprintf("corpusFiles[%d] = ARG%d", i, i+1);
  corpusNames[i] = corpusFiles[i][:strstrt(corpusFiles[i], ".")-1]
}
print corpusNames
cd ARG1

ymax = 0
maxBlockSize = 0
maxBlock_max_y = 0
maxSample = 0
bias = 0

fmt = "% 14s :% 6.2f"
format = fmt . "\n".fmt . "\n".fmt . "\n".fmt . "\n".fmt . "\n".fmt
format = bias == 0 ? format : format . "\n".fmt

# Harcoded for now

xpos = 0
#if(ARGC != 2) {
#    set xrange[0:]
#}

array arrBlockSize[ARGC-1]
array infostat[ARGC-1]
array xposArr[ARGC-1]
array max_y[ARGC-1]
array boxsize[ARGC-1]
array compMeanRatio[ARGC-1]
array violinPos[ARGC-1]
array meanPos[ARGC-1]
array xscale[ARGC-1]
array medianValue[ARGC-1]
array lowQuartileValue[ARGC-1]
array upQuartileValue[ARGC-1]
array posMax[ARGC-1]
array posMin[ARGC-1]

i=1
do for [i=1:ARGC-1] {

   print sprintf("Processing file: %s", corpusFiles[i])

# This will compute the block size when the data is read from the statistic file. This can be computed from the statistic that 
# include a row for every possible compression size of a block from 1 to block size + header. Note that expansion is possible when a block
# is all literal. In which case the chunk increase by the header size: 15. Maximum block size is computed for the case of multiple
# violon plot that don't have the same block size for compression.

   stats corpusFiles[i] nooutput
   numRecord = STATS_records
   chunkSize = numRecord - 15.0
   arrBlockSize[i] = chunkSize
   maxBlockSize = maxBlockSize < chunkSize ? chunkSize : maxBlockSize

   set datafile separator comma

   array M[numRecord]
   array N[numRecord]

# First column is the value of the compression size for a block. Values range from 1 to block size
# Third column is the frequency of apperance of the compression size for the full file. Application does normalise this frequency
# to prevent a frequency value that is too large when the file data contains many GBytes.
   
   stats corpusFiles[i] using (M[int($0 + 1)] = $1) name "M" nooutput
   stats corpusFiles[i] using (N[int($0 + 1)] = $3) name "N" nooutput

   array M_x_N[numRecord]
   array Bias_M_x_N[numRecord]
   
   bias = chunkSize / 2

# The next computation is used to create intermediate value array to compute statistic: mean, variance, median....
   
   stats N using (M_x_N[int($0 + 1)] = N[int($0 + 1)] * M[int($0 + 1)]) name "M_x_N" nooutput
   stats N using (M[$0 + 1] <= bias ? N[int($0 + 1)] * M[int($0 + 1)] : N[int($0 + 1)] * chunkSize) name "M_x_N_B" nooutput
   stats N using (M[$0 + 1] <= bias ? N[int($0 + 1)] : N[int($0 + 1)]) name "B_N" nooutput
   
   compBiasMeanRatio = chunkSize / (M_x_N_B_sum / B_N_sum)
   compMeanSize = M_x_N_sum / N_sum
   compMeanRatio[i] = chunkSize / compMeanSize
   
   array M_mean_x_N_sqrt[numRecord]
   stats M using (M_mean_x_N_sqrt[int($0 + 1)] = N[int($0 + 1)] * ((($1)-compMeanSize) **2)) name "M_mean_x_N_2" nooutput
   
   compVariance = sqrt(M_mean_x_N_2_sum / (N_sum - 1))
   compVarianceHigh = chunkSize / (compMeanSize - compVariance)
   compVarianceLow = chunkSize / (compMeanSize + compVariance)
   
   posMedian = N_sum / 2.0
   posUpQuartile = N_sum / 4.0
   posLowQuartile = posMedian + posUpQuartile
   
   isLastMedian = 0
   isLastLowQuartile = 0
   isLastUpQuartile = 0
   
   medianValue[i] = 0
   upQuartileValue[i] = 0
   lowQuartileValue[i] = 0
   cumTotal = 0
   
   posMax[i] = 0
   posMin[i] = numRecord
   
   array cumulMedian[numRecord]

# This will loop through every row of the statistic file and will be used to compute the
# median ratio, the upper quartile ratio and the lower quartible ratio. This loop is
# based on a similar computation done by the Excel Spreadsheet at this URL:
# https://rambus.sharepoint.com/:x:/r/sites/spointerconnectbu/Departments/Architecture/Data%20Compression/Statistical%20Analysis/lz4_silicia_corpus.txt_4096_stat.xlsx
   
   do for[col = 1:numRecord]{
   
      cumulMedian[col] = cumTotal + N[col]
      cumTotal = cumTotal + N[col]
   
      if (N[col] != 0) {
         posMin[i] = col < posMin[i] ? col : posMin[i]
         posMax[i] = posMax[i] < col ? col : posMax[i]
      }
   
      isMedian = cumulMedian[col] < posMedian ? 0 : 1
      isLowQuartile = cumulMedian[col] < posLowQuartile ? 0 : 1
      isUpQuartile = cumulMedian[col] < posUpQuartile ? 0 : 1
   
      if (isUpQuartile != isLastUpQuartile) {
         upQuartileValue[i] = col - 0.0
      }
   
      if (isMedian != isLastMedian) {
         medianValue[i] = col - 0.0
      }
   
      if (isLowQuartile != isLastLowQuartile) {
         lowQuartileValue[i] = col - 0.0
      }
      isLastMedian = isMedian
      isLastLowQuartile = isLowQuartile
      isLastUpQuartile = isUpQuartile
   }   
   ratioMedian = chunkSize / medianValue[i]
   ratioUpQuartile = chunkSize / upQuartileValue[i]
   ratioLowQuartile = chunkSize / lowQuartileValue[i]

   maxRatioValue = chunkSize / posMin[i]
   minRatioValue = chunkSize / posMax[i]
   
# This will create a distribution of the statistic with a sample for every block that was compressed. The value
# of the sample is the compression size of the block. The number of sample created for every row is the frequency
# retrieved from the output statistic.

# WARNING. The size array of V will depends on the number of block that was compressed. The bigger the size of the
# file the greater the number of samples. This is why the application "normalize" the number of block that is compressed
# to a number of sample that is not greater than 64K. Otherwise the CPU processing time to create the smoothing can
# grow exponentially.

   array V[N_sum]
   numSample = 1
   
   do for[col = 1:numRecord]{
      if (N[col] != 0) {
         do for[num = 1:(N[col])] {
            V[numSample] = M[col]
            numSample = numSample + 1
         }
      }
   }
   
# Now create the distribution using array V and store it into $Data

   set table $Data separator comma
   set samples numSample - 1
   n = 0
   plot for[i = 1:1] '+' u(n = n + 1) :(V[n]) w table
   unset table
 

# From an Electrical Engineering terminology the kernel density function is the transfer resampling function of a normal 
# distribution (also a low pass filter). Thus, the second column of the Gnuplot smooth density table is the output of the 
# filtering of the samples done by convoluting the input samples with the low pass transfer resampling function. The 
# bandwidth value create the "spreading" shape of the normal distribution transfer function that will determine how much 
# samples contribute to the resampling value and thus the smoothing (or low pass bandwidth cutoff). The Bandwidth value 
# has been computed with "try and check" and provide a good "smoothing" with 10 -> block size of 4096 and 2 -> block size
# of 512. For now the bandwidth required for a block size will use an linear extrapolation algorithm.
#
# https://aakinshin.net/posts/kde-bw/
#
# Linear formula for Banwdith value. Wanted: 10 -> 4095, 2 -> 512
# y = slope*(x) + origin -> origin = y - slope*(x)
# Bandwidth is y

   slope = (10.0 - 2.0)/(4096.0 - 512)
   origin = 10 - slope*4096
   chunkBandwidth = slope*chunkSize + origin

# Resampling using Kernel Density Estimation of the distribution $Data and output into $kdensity table.

   set table $kdensity
   set samples(numSample - 1)
   plot $Data using 2:(1) smooth kdensity bandwidth chunkBandwidth
   unset table
    
   set datafile separator whitespace

# This provide a mean to put into a file the result of the Kernel Density Estimation smoothering for plotting
# purpose

   statfile(i) = sprintf("statfile_%d.txt", i)
   set table statfile(i)
   plot $kdensity using 1:2
   unset table

# Use the statistic to find the maximum filtered resampling value from the smooth operation. This will provide
# the maximum filtered sample for each file and will be use to adjust and position the plot for each file.

   stats [*:*][*:*] $kdensity nooutput  
   max_y[i] = STATS_max_y 
   boxsize[i] = max_y[i] * 0.03   
   maxBlock_max_y = maxBlock_max_y < STATS_max_y ? STATS_max_y : maxBlock_max_y

   if(isDebug) { print "chunkSize = ", chunkSize, ", STATS_max_y = ", STATS_max_y, ", maxBlock_max_y = ", maxBlock_max_y }

   if((ARGC-1) < 2) {

      infostat[i] = sprintf(format, "Mean", compMeanRatio[i], "Low Variance", compVarianceLow, "High Variance", compVarianceHigh, "Median", ratioMedian, "First Quartile", ratioLowQuartile, "Third Quartile", ratioUpQuartile)
      infostat[i] = bias == 0 ? infostat[i] : sprintf(format, "Mean", compMeanRatio[i], "Mean (Bias)", compBiasMeanRatio, "Low Variance", compVarianceLow, "High Variance", compVarianceHigh, "Median", ratioMedian, "First Quartile", ratioLowQuartile, "Third Quartile", ratioUpQuartile)

      set style textbox 2 opaque fc rgb 0xb0b0b0 margin 4,4
      set label 30 infostat[i] at graph .95,1.0 right boxed bs 2 front font "Courier New"
   }
}

set style fill solid 0.7
set boxwidth boxsize[1] 

intMaxColor = int(100.0*(maxBlockSize /( maxBlockSize+15)))
maxColor = intMaxColor / 100.0

set palette defined ( 0.0 'web-green', .5 'goldenrod', 0.8 'red', maxColor 'black')
set colorbox invert
set cbrange[0:maxColor]
set cbtics (sprintf("%2.2f", maxColor) maxColor, "2.0" 0.5, "5.0" .2, "10.0" .1, "\U+221E" 0.0)
set cblabel "Compression Ratio of Page Blocks"

if(!isColorbox) {
   unset colorbox
}
set style textbox 2 opaque fc rgb 0xb0b0b0 margin 4, 4
ymax = maxBlockSize > 512 ? maxBlockSize + int(maxBlockSize / 20) : maxBlockSize + 50

do for[i=1:ARGC-1] {
    violinPos[i] = int(maxBlockSize/compMeanRatio[i] - arrBlockSize[i]/compMeanRatio[i])
    meanPos[i] = int(maxBlockSize/compMeanRatio[i])
    if(isPosBottom ) {
        violinPos[i] = int(maxBlockSize - arrBlockSize[i])
        meanPos[i] = int(violinPos[i] + (arrBlockSize[i]/compMeanRatio[i]))
    }
    if(isPosTop) {
        violinPos[i] = 0
        meanPos[i] = int(arrBlockSize[i]/compMeanRatio[i])
    }
}
# Compute the space allocated between each violon plot. This is chosen to be 20% of the maximum width of
# the violon plot having maximum width.

xspace = maxBlock_max_y * 0.10
xpos = xspace

do for[i=1:ARGC-1] {

# A violon plot uses the smoothered values from the resampling function and use a negative and positive relative horizontal vector 
# having the length of the smoothered value from an absolute position on the x axis. A scaling value for the vector is computed
# to normalize the violon plot of each statistic file to provide a better visual plotting comparison. An absolute (xpos) position
# of the violon plot is computed for each statistic file and some spacing is added to distance each violon plot from each other

   xscale[i] = maxBlock_max_y / max_y[i]
   xpos = xpos + maxBlock_max_y
   xposArr[i] = xpos
   xpos = xpos + maxBlock_max_y + xspace

   if((ARGC-1) >= 2) {
      set object 11+i*2 circle at first xposArr[i],meanPos[i] radius char 0.5 fillcolor rgb 'black' fillstyle solid border lt -1 lw 2 front
   }

   if(isYorigTop) {
      ypos = ymax
      yoff = -1
      if(isInfoTop) {
         ypos = violinPos[i]+posMin[i]
         yoff = 3
      }
   }
   else {
      ypos = 0
      yoff = -1    
   }
   set label 11+i*2+1 sprintf("%s", corpusNames[i])."\nBlock Size = ".sprintf("%d", arrBlockSize[i])."\nAvg Ratio = ".sprintf("%2.2f", compMeanRatio[i]) at xposArr[i], ypos center font 'Arial,10' front offset character 0,yoff noenhanced
   if(isDebug) { print sprintf("%s", corpusNames[i])."\nBlock Size = ".sprintf("%d", arrBlockSize[i])."\nAvg Ratio = ".sprintf("%2.2f", compMeanRatio[i]).", xposArr[i] = ".sprintf("%d", xposArr[i]).", ypos = ".sprintf("%d", ypos) }
}
xposMax = xpos
set xrange[0:xposMax]

if(isDebug) { print "maxBlock_max_y is ", maxBlock_max_y }
if(isDebug) { print "xposArr is ", xposArr }
if(isDebug) { print "xscale is ", xscale }

if((ARGC-1) >= 2) {

   set table $Curve
   set samples ARGC-1
   plot sample [n=1:ARGC-1] '+' using (xposArr[n]):(meanPos[n])
   unset table
}

set border 3
unset key
set bmargin 5
set tmargin 5

set xtics nomirror
set ytics nomirror
set xlabel "Density Of Page Blocks Compressed"
set ylabel "Compression BLock Size"

set xtics ("" 0) nomirror
do for[i=1:ARGC-1] {
   set xtics add (xposArr[i]) nomirror
}

if(!isXtics) {    
    unset xlabel
    set format x ""
}

if(isYorigTop) {
   set yrange[ymax:0]
}
else {
  set yrange[0:ymax]
}

if((ARGC-1) < 2) {
   i=1
   set boxwidth boxsize[i]/2 
   set object 20+i*2 rect from -(boxsize[i]/2)+xposArr[i], violinPos[i]+lowQuartileValue[i] to (boxsize[i]/2)+xposArr[i], violinPos[i]+upQuartileValue[i] fc lt -1 lw 2 front
   set obj 20+i*2 fillstyle solid border -1 
   set obj 20+i*2 fillcolor "dark-violet"
   set arrow 40+i*2 from xposArr[i]-boxsize[i]/2,violinPos[i]+medianValue[i] to xposArr[i]+boxsize[i]/2,violinPos[i]+medianValue[i] nohead lt -1 lw 2 front
}

if((ARGC-1) >= 2) {
   if(isCurve) {
      plot for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):($2*xscale[j]):(0):($1/arrBlockSize[j]) with vectors nohead lc palette z, for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):(-$2*xscale[j]):(0):($1/arrBlockSize[j]) with vectors nohead lc palette z, \
      $Curve smooth csplines lt -1 lw 2
   }
   else {
      plot for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):($2*xscale[j]):(0):($1/arrBlockSize[j]) with vectors nohead lc palette z, for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):(-$2*xscale[j]):(0):($1/arrBlockSize[j]) with vectors nohead lc palette z
   }
}
else {
   plot for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):($2*xscale[j]):(0):($1/arrBlockSize[j]) with vectors nohead lc palette z, for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):(-$2*xscale[j]):(0):($1/arrBlockSize[j]) with vectors nohead lc palette z, \
   for[j=1:ARGC-1] '+' every ::::0 using (xposArr[j]):(violinPos[j]+lowQuartileValue[j]):(violinPos[j]+posMin[j]):(violinPos[j]+posMax[j]):(violinPos[j]+upQuartileValue[j]) with candlesticks fs solid lt 1 lw 2 notitle whiskerbars
}
