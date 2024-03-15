
reset session
set encoding utf8
set datafile separator comma
cd 'C:\Users\gbonneau\git\smallz4'
corpusFile = "lz4_silicia_corpus.txt_4096.csv"
stats corpusFile nooutput
numRecord = STATS_records
chunkSize = numRecord-15.0

array M[numRecord]
array N[numRecord]

stats corpusFile using (M[int($0+1)] = $1) name "M" nooutput
stats corpusFile using (N[int($0+1)] = $2) name "N" nooutput
array M_x_N[numRecord]
array Bias_M_x_N[numRecord]

bias = 2048.0

stats N using (M_x_N[int($0+1)] = N[int($0+1)]*M[int($0+1)]) name "M_x_N" nooutput
#stats N using (M[$0+1] <= bias ? N[int($0+1)]*M[int($0+1)] : 0) name "M_x_N_B" nooutput
#stats N using (M[$0+1] <= bias ? N[int($0+1)] : 0) name "B_N" nooutput

stats N using (M[$0+1] <= bias ? N[int($0+1)]*M[int($0+1)] : N[int($0+1)]*chunkSize) name "M_x_N_B" nooutput
stats N using (M[$0+1] <= bias ? N[int($0+1)] : N[int($0+1)]) name "B_N" nooutput

compBiasMeanRatio = chunkSize / (M_x_N_B_sum/B_N_sum)
compMeanSize = M_x_N_sum / N_sum
compMeanRatio = chunkSize / compMeanSize

array M_mean_x_N_sqrt[numRecord]
stats M using (M_mean_x_N_sqrt[int($0+1)] = N[int($0+1)]*((($1)-compMeanSize)**2)) name "M_mean_x_N_2" nooutput

compVariance = sqrt(M_mean_x_N_2_sum/(N_sum-1))
compVarianceHigh = chunkSize/(compMeanSize - compVariance)
compVarianceLow = chunkSize/(compMeanSize + compVariance)

posMedian = N_sum/2.0
posUpQuartile = N_sum/4.0
posLowQuartile = posMedian + posUpQuartile

isLastMedian      = 0
isLastLowQuartile = 0
isLastUpQuartile  = 0

medianValue       = 0
upQuartileValue   = 0
lowQuartileValue  = 0
cumTotal          = 0

posMax = 0;
posMin = numRecord; 

array cumulMedian[numRecord]

do for [col=1:numRecord] {

   cumulMedian[col] = cumTotal + N[col]
   cumTotal = cumTotal + N[col]

   if(N[col] != 0) {
      posMin = col < posMin ? col : posMin
      posMax = posMax < col ? col : posMax
   }

   isMedian = cumulMedian[col] < posMedian ? 0 : 1 
   isLowQuartile = cumulMedian[col] < posLowQuartile ? 0 : 1
   isUpQuartile = cumulMedian[col] < posUpQuartile ? 0 : 1

   if(isUpQuartile != isLastUpQuartile) {
      upQuartileValue = col-0.0
   }

   if(isMedian != isLastMedian) {
      medianValue = col-0.0
   }

   if(isLowQuartile != isLastLowQuartile) {
      lowQuartileValue = col-0.0
   }
   isLastMedian = isMedian
   isLastLowQuartile = isLowQuartile
   isLastUpQuartile = isUpQuartile
}

print posMin
print posMax

ratioMedian = chunkSize/medianValue
ratioUpQuartile = chunkSize/upQuartileValue
ratioLowQuartile = chunkSize/lowQuartileValue
ratioVariance = chunkSize/compVariance

print medianValue
print upQuartileValue
print lowQuartileValue

maxRatioValue = chunkSize/posMin
minRatioValue = chunkSize/posMax

array V[N_sum]
numSample = 1

do for [col=1:numRecord] {
   if(N[col] != 0) {
      do for [num=1:(N[col])] {      
         V[numSample] = M[col] 
         numSample=numSample+1
      }
   }
}

set table $Data separator comma
    set samples numSample-1
    n = 0
    plot for [i=1:1] '+' u (n=n+1):(V[n]) w table
unset table

stats $Data name "D" nooutput

set table $kdensity
    set samples (numSample-1)
    plot $Data using 2:(1) smooth kdensity bandwidth 10
unset table

stats $kdensity name "K" nooutput

set datafile separator whitespace
set border 3

set xtics nomirror
set label 8
set ytics nomirror
unset key

set style fill solid 0.7 
set boxwidth 2.5

xviolin = 0.0

set palette defined ( 0.0 'web-green', .5 'goldenrod', 0.8 'red', 1.0 'black')
set cbtics ("0.98" 1.0, "2.0" 0.5, "5.0" .2, "10.0" .1, "\U+221E" 0.0125)

fmt  = "% 14s :% 6.2f"
format = fmt . "\n" . fmt . "\n" . fmt . "\n" . fmt . "\n" . fmt . "\n" . fmt 
format = bias == 0 ? format : format . "\n" . fmt

infostat = sprintf(format, "Mean", compMeanRatio, "Low Variance", compVarianceLow, "High Variance", compVarianceHigh, "Median", ratioMedian, "First Quartile", ratioLowQuartile, "Third Quartile", ratioUpQuartile)

infostat = bias == 0 ? infostat : sprintf(format, "Mean", compMeanRatio, "Mean (Bias)", compBiasMeanRatio, "Low Variance", compVarianceLow, "High Variance", compVarianceHigh, "Median", ratioMedian, "First Quartile", ratioLowQuartile, "Third Quartile", ratioUpQuartile)


set style textbox 2 opaque fc rgb 0xb0b0b0 margin 4,4
set label 10 infostat at graph .95,1.0 right boxed bs 2 front font "Courier New"

ymax = chunkSize + int(chunkSize/20)
set yrange[ymax:0]
set bmargin 3

set title font ',13'
set title "Silesia Corpus\nSize = ".sprintf("%d", chunkSize)
set xlabel "Density Of Page Chunks Compressed"
set ylabel "Compressed size of Page Chunks"
set cblabel "Compression Ratio of Page Chunks"

set object 10 rect from -1.25,lowQuartileValue to 1.25,upQuartileValue fc lt -1 lw 2 front
set obj 10 fillstyle empty border -1 front

plot $kdensity using (0):1:($2):(0):($1/4096.0) with vectors lc palette z, '' using (0):1:(-$2):(0):($1/4096) with vectors lc palette, \
'+' every ::::0 using (xviolin):(lowQuartileValue):(posMin):(posMax):(upQuartileValue) with candlesticks fs solid lt 1 lw 2 notitle whiskerbars, \
'+' every ::::0 using (xviolin):(medianValue):(medianValue):(medianValue):(medianValue) with candlesticks fs solid lt -1 lw 2 notitle

