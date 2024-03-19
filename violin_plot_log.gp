
reset session
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

stats N using (M_x_N[int($0+1)] = N[int($0+1)]*M[int($0+1)]) name "M_x_N" nooutput

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
         V[numSample] = chunkSize/M[col] 
         numSample=numSample+1
      }
   }
}

set table $Data separator comma
    set samples numSample-1
    n = 0
    plot for [i=1:1] '+' u (n=n+1):(V[n]) w table
unset table

set table $kdensity
    set samples (numSample-1)
    plot $Data using 2:(1) smooth kdensity bandwidth 0.01
unset table

set datafile separator whitespace
#set xrange[-50:50]
set border 2
set yrange[0.95:150]
set xtics ("Silesia Corpus\nSize = 4096" 0) offset 0,0
set xtics nomirror scale 0
set xtics offset 0,2
set ytics nomirror
unset key

set style fill solid 0.7 
set boxwidth 2.5
set logscale y

set ytics (1,2,3,4,5,6,7,8,9,10,25,50,100)

xviolin = 0.0

infostat = sprintf("Mean :  \nVariance :  \nMedian :  \nFirst Quartile :  \nThird Quartile :  ") 
infonumber = sprintf(" %6.2f\n %6.2f\n %6.2f\n %6.2f\n %6.2f", compMeanRatio, compVariance, ratioMedian, ratioLowQuartile, ratioUpQuartile)
infobox    = sprintf("\n\n\n\nThird Quartile :         ")
 
set style textbox 2 opaque fc rgb 0xffff00 noborder
set style textbox 3 transparent fc rgb 0xffff00 border lc "black"

set label 10 infostat at graph .75,.8 right boxed bs 2 front
set label 11 infonumber at graph .75,.8 left boxed bs 2 front
#set label 12 infobox at graph .75,.8 left boxed bs 3 front

plot $kdensity using ($2/1000.):1 w filledcurve x=0 lt 2, '' using (- $2/1000.):1 w filledcurve x=0 lt 2, \
'+' every ::::0 using (xviolin):(ratioLowQuartile):(minRatioValue):(maxRatioValue):(ratioUpQuartile) with candlesticks fs solid lt 1 lw 2 notitle whiskerbars, \
'+' every ::::0 using (xviolin):(ratioMedian):(ratioMedian):(ratioMedian):(ratioMedian) with candlesticks fs solid lt -1 lw 2 notitle whiskerbars

