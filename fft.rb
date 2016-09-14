#!/usr/bin/env ruby

niter = ARGV[0]
num_threads = [2, 4, 8, 16, 24, 48]

puts 'unit: sec'
puts "# of threads: #{num_threads.join(' ')}"
puts

def run(command, num_iterations)
  iter_cmd = "for i in $(seq #{num_iterations}); do #{command}; done"
  stack_size_kb = 16 * 1024
  result = `ulimit -s #{stack_size_kb}; export OMP_STACKSIZE=#{stack_size_kb}; #{iter_cmd}`
    .scan(/Time Program\s*= ([0-9\.]+)/)
    .flatten
    .map(&:to_f)
    .min
  puts result
end

puts 'serial'
run 'bin/fft.icc.serial -n 268435456', niter
puts

puts 'untied'
num_threads.each do |n|
  run "OMP_NUM_THREADS=#{n} bin/fft.icc.omp-tasks -n 268435456", niter
end
puts

puts 'tied'
num_threads.each do |n|
  run "OMP_NUM_THREADS=#{n} bin/fft.icc.omp-tasks-tied -n 268435456", niter
end
