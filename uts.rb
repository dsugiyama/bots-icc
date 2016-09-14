#!/usr/bin/env ruby

niter = ARGV[0]
num_threads = [2, 4, 8, 16, 24, 48]

puts 'unit: nodes/sec'
puts "# of threads: #{num_threads.join(' ')}"
puts

def run(command, num_iterations)
  iter_cmd = "for i in $(seq #{num_iterations}); do #{command}; done"
  stack_size_kb = 16 * 1024
  result = `ulimit -s #{stack_size_kb}; export OMP_STACKSIZE=#{stack_size_kb}; #{iter_cmd}`
    .scan(/Nodes\/Sec\s*= ([0-9\.]+)/)
    .flatten
    .map(&:to_f)
    .max
  puts result
end

puts 'serial'
run 'bin/uts.icc.serial -f inputs/uts/small.input', niter
puts

puts 'untied'
num_threads.each do |n|
  run "OMP_NUM_THREADS=#{n} bin/uts.icc.omp-tasks -f inputs/uts/small.input", niter
end
puts

puts 'tied'
num_threads.each do |n|
  run "OMP_NUM_THREADS=#{n} bin/uts.icc.omp-tasks-tied -f inputs/uts/small.input", niter
end