
reset session
set encoding utf8
set term qt size 1010,1000
set term png enhanced truecolor size 1010,1000

ARGC = 2
ARG1 = 'C:\Users\gbonneau\git\smallz4'
ARG2 = "lz4_enwik9.txt_4096.csv"

set output ARG2.sprintf(".png")

cd ARG1

corpusTitle = "Wikipedia Corpus"
array corpusFiles[ARGC-1]

numARG = (9 < ARGC) ? 8 : ARGC-1

do for [i=1:numARG] {
  eval sprintf("corpusFiles[%d] = ARG%d", i, i+1);
}

ymax = 0
maxChunkSize = 0
bias = 0

fmt = "% 14s :% 6.2f"
format = fmt . "\n".fmt . "\n".fmt . "\n".fmt . "\n".fmt . "\n".fmt
format = bias == 0 ? format : format . "\n".fmt

# Harcoded for now

xpos = 10
if(ARGC != 2)) {
    set xrange[0:]
}
if(ARGC != 2) {
   set title font ',13'
   set title sprintf("%s Set", corpusTitle)
}

array arrChunkSize[5]
array infostat[5]
array xposArr[5]
array max_y[5]
array boxsize[5]
array compMeanRatio[5]
array violinPos[5]
array yscale[5]
array medianValue[5]
array lowQuartileValue[5]
array upQuartileValue[5]
array posMax[5]
array posMin[5]

i=1
do for [i=1:ARGC-1] {

   print sprintf("Processing Loop = %d", i)

   stats corpusFiles[i] nooutput
   numRecord = STATS_records
   chunkSize = numRecord - 15.0
   arrChunkSize[i] = chunkSize
   maxChunkSize = maxChunkSize < chunkSize ? chunkSize : maxChunkSize

   set datafile separator comma

   array M[numRecord]
   array N[numRecord]
   
   stats corpusFiles[i] using (M[int($0 + 1)] = $1) name "M" nooutput
   stats corpusFiles[i] using (N[int($0 + 1)] = $3) name "N" nooutput

   array M_x_N[numRecord]
   array Bias_M_x_N[numRecord]
   
   bias = chunkSize / 2
   
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
   
   set table $Data separator comma
   set samples numSample - 1
   n = 0
   plot for[i = 1:1] '+' u(n = n + 1) :(V[n]) w table
   unset table
   
   stats $Data name "D" nooutput

#preliminary linear formula for Banwdith value. Wanted: 10 -> 4095, 2 -> 512
# y = slope*(x) + origin -> origin = y - slope*(x)
# bandwidth is y

   slope = (10.0 - 2.0)/(4096.0 - 512)
   origin = 10 - slope*4096
   chunkBandwidth = slope*chunkSize + origin

   set table $kdensity
   set samples(numSample - 1)
   plot $Data using 2:(1) smooth kdensity bandwidth chunkBandwidth
   unset table
     
   set datafile separator whitespace

   statfile(i) = sprintf("statfile_%d.txt", i)

   set table statfile(i)
   plot $kdensity using 1:2
   unset table

   stats [*:*][*:*] $kdensity nooutput
   
   if(chunkSize == maxChunkSize) {
    maxChunk_max_y = STATS_max_y
   }
   max_y[i] = STATS_max_y 
   boxsize[i] = max_y[i] * 0.03

   if((ARGC-1) < 3) {

      infostat[i] = sprintf(format, "Mean", compMeanRatio[i], "Low Variance", compVarianceLow, "High Variance", compVarianceHigh, "Median", ratioMedian, "First Quartile", ratioLowQuartile, "Third Quartile", ratioUpQuartile)
      infostat[i] = bias == 0 ? infostat[i] : sprintf(format, "Mean", compMeanRatio[i], "Mean (Bias)", compBiasMeanRatio, "Low Variance", compVarianceLow, "High Variance", compVarianceHigh, "Median", ratioMedian, "First Quartile", ratioLowQuartile, "Third Quartile", ratioUpQuartile)

      set style textbox 2 opaque fc rgb 0xb0b0b0 margin 4,4
      set label 30 infostat[i] at graph .95,1.0 right boxed bs 2 front font "10,Courier New"
   }
}

set border 3
set xtics nomirror
set ytics nomirror
unset key

set style fill solid 0.7
set boxwidth boxsize[1] 

intMaxColor = int(100.0*(maxChunkSize /( maxChunkSize+15)))
maxColor = intMaxColor / 100.0

set palette defined ( 0.0 'web-green', .5 'goldenrod', 0.8 'red', maxColor 'black')
set colorbox invert
set cbrange[0:maxColor]
set cbtics (sprintf("%2.2f", maxColor) maxColor, "2.0" 0.5, "5.0" .2, "10.0" .1, "\U+221E" 0.0)
set style textbox 2 opaque fc rgb 0xb0b0b0 margin 4, 4

ymax = maxChunkSize > 512 ? maxChunkSize + int(maxChunkSize / 20) : maxChunkSize + 50

do for[i=1:ARGC-1] {
    violinPos[i] = int(maxChunkSize/compMeanRatio[i] - arrChunkSize[i]/compMeanRatio[i])
}

do for[i=1:ARGC-1] {
    yscale[i] = maxChunk_max_y / max_y[i]
    xpos = xpos + maxChunk_max_y
    xposArr[i] = xpos
    xpos = xpos + maxChunk_max_y + 10

    if((ARGC-1) > 2) {
        set object 11+i*2 circle at first xposArr[i],violinPos[i]+(arrChunkSize[i]/compMeanRatio[i]) radius char 0.5 fillcolor rgb 'black' fillstyle solid border lt -1 lw 2 front
    }
    set label 11+i*2+1 sprintf("%s", corpusTitle)."\nChunk Size = ".sprintf("%d", arrChunkSize[i])."\nComp Ratio = ".sprintf("%2.2f", compMeanRatio[i]) at xposArr[i], violinPos[i] center font '10,Courier New' front offset character 0,3
}

set yrange[ymax:0]
set bmargin 3
set tmargin 5

set xlabel "Density Of Page Chunks Compressed"
set ylabel "Delta Chunk size of Compression"
set cblabel "Compression Ratio of Page Chunks"

if((ARGC-1) > 2) {

    set table $Curve
    set samples ARGC-1
    plot sample [n=1:ARGC-1] '+' using (xposArr[n]):(violinPos[n]+(arrChunkSize[n]/compMeanRatio[n]))
    unset table
}

if((ARGC-1) < 3) {
   i=1
   set boxwidth boxsize[i]/2 
   set object 20+i*2 rect from -(boxsize[i]/2)+xposArr[i], violinPos[i]+lowQuartileValue[i] to (boxsize[i]/2)+xposArr[i], violinPos[i]+upQuartileValue[i] fc lt -1 lw 2 front
   set obj 20+i*2 fillstyle solid border -1 
   set obj 20+i*2 fillcolor "dark-violet"
   set arrow 40+i*2 from xposArr[i]-boxsize[i]/2,violinPos[i]+medianValue[i] to xposArr[i]+boxsize[i]/2,violinPos[i]+medianValue[i] nohead lt -1 lw 2 front
}

if((ARGC-1) > 2) {
   plot for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):($2*yscale[j]):(0):($1/arrChunkSize[j]) with vectors nohead lc palette z, for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):(-$2*yscale[j]):(0):($1/arrChunkSize[j]) with vectors nohead lc palette z, \
   $Curve smooth csplines lt -1 lw 2
}
else {
   plot for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):($2*yscale[j]):(0):($1/arrChunkSize[j]) with vectors nohead lc palette z, for[j=1:ARGC-1] statfile(j) using (xposArr[j]):($1+violinPos[j]):(-$2*yscale[j]):(0):($1/arrChunkSize[j]) with vectors nohead lc palette z, \
   for[j=1:ARGC-1] '+' every ::::0 using (xposArr[j]):(violinPos[j]+lowQuartileValue[j]):(posMin[i]):(violinPos[j]+posMax[j]):(violinPos[j]+upQuartileValue[j]) with candlesticks fs solid lt 1 lw 2 notitle whiskerbars
}
